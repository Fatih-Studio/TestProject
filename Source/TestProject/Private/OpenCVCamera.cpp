#include "Public/OpenCVCamera.h"

AOpenCVCameraActor::AOpenCVCameraActor()
{	
	PrimaryActorTick.bCanEverTick = true;
	PlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plane Mesh"));
	RootComponent = PlaneMesh;
}

void AOpenCVCameraActor::BeginPlay()
{
	Super::BeginPlay();
	
	if (BaseMaterial)
	{
		DynMaterial = UMaterialInstanceDynamic::Create(BaseMaterial,this);
		PlaneMesh->SetMaterial(0,DynMaterial);
	}
	
	Camera.open(0);
	if (!Camera.isOpened())
	{
		
		UKismetSystemLibrary::PrintString(this, TEXT("Unable to open camera"), true, true, FLinearColor::Red, 5.0f);
		return;
	}
	
	Camera >> Frame;
	if (!Frame.empty())
	{
		InitCameraTexture(Frame.cols, Frame.rows);
	}

	// Fix exposure and focus to prevent hunting
	Camera.set(cv::CAP_PROP_AUTOFOCUS, 0);  // disable autofocus
	Camera.set(cv::CAP_PROP_AUTO_EXPOSURE, 0); // disable auto exposure
	Clahe = cv::createCLAHE(4.0, cv::Size(8, 8));
	
	cv::aruco::Dictionary Dictionary = cv::aruco::getPredefinedDictionary(GetOpenCVDictionaryType(DictionaryGrid, DictionarySize));
	cv::aruco::DetectorParameters Params;
	
	MarkerIds.reserve(100);
	MarkerCorners.reserve(100);
	RejectedCorners.reserve(100);
	
	ArucoDetector = cv::aruco::ArucoDetector(Dictionary, Params);
	
	UKismetSystemLibrary::PrintString(this, TEXT("Camera opened successfully"), true, true, FLinearColor::Green, 5.0f);
}	

void AOpenCVCameraActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	ArucoTimer += DeltaTime;;

	if (ArucoTimer < 1.0f/ArucoUpdateRate)
	{
		return;
	}

	ArucoTimer = 0.0f;

	MarkerIds.clear();
	MarkerCorners.clear();
	RejectedCorners.clear();
		
	Camera >> Frame;

	if (Frame.empty())
	{
		return;
	}
	
	if (!bCalibrated)
	{
		DetectOnAllCandidates(Frame);
	}
	else
	{
		DetectMarkers(Frame);
		
		// If markers lost for too long, recalibrate
		if (MarkerIds.size() == 0)
		{
			FramesWithNoDetection++;

			if (FramesWithNoDetection >= RecalibrationThreshold)
			{
				UE_LOG(LogTemp, Warning, TEXT("Markers lost — recalibrating channel"));
				bCalibrated = false;
				BestCandidateIndex = -1;
				FramesWithNoDetection = 0;
			}
		}
		else
		{
			FramesWithNoDetection = 0;
		}
	}

	cv::Mat BGRAFrame;
	cv::cvtColor(Frame,BGRAFrame,cv::COLOR_BGR2BGRA);
	UpdateTextureFromMat(BGRAFrame);
}

void AOpenCVCameraActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetActorTickEnabled(false);
	
		if (Camera.isOpened())
		{
			try
			{
				Camera.release();
			}
			catch (...)
			{
			}
		}
	
	for (auto& Mat : MarkerCorners)   { Mat.release(); }
	for (auto& Mat : RejectedCorners) { Mat.release(); }

	// Now safe — outer vector buffers were from UE's allocator (via reserve)
	MarkerCorners.clear();
	RejectedCorners.clear();
	MarkerIds.clear();
	
	Frame.release();
	selected_frame.release();;
	
		
	Super::EndPlay(EndPlayReason);
}

cv::aruco::PredefinedDictionaryType AOpenCVCameraActor::GetOpenCVDictionaryType(EOpenCVArucoDictionary Dictionary,EOpenCVArucoDictionarySize Size)
{
	// Original has no size variant
	if (Dictionary == EOpenCVArucoDictionary::DictOriginal)
	{
		return cv::aruco::DICT_ARUCO_ORIGINAL;
	}

	// Build lookup table [Dictionary][Size]
	const cv::aruco::PredefinedDictionaryType LookupTable[4][4] =
	{
		// Size50               Size100               Size250               Size1000
		{ cv::aruco::DICT_4X4_50,  cv::aruco::DICT_4X4_100,  cv::aruco::DICT_4X4_250,  cv::aruco::DICT_4X4_1000  }, // 4x4
		{ cv::aruco::DICT_5X5_50,  cv::aruco::DICT_5X5_100,  cv::aruco::DICT_5X5_250,  cv::aruco::DICT_5X5_1000  }, // 5x5
		{ cv::aruco::DICT_6X6_50,  cv::aruco::DICT_6X6_100,  cv::aruco::DICT_6X6_250,  cv::aruco::DICT_6X6_1000  }, // 6x6
		{ cv::aruco::DICT_7X7_50,  cv::aruco::DICT_7X7_100,  cv::aruco::DICT_7X7_250,  cv::aruco::DICT_7X7_1000  }, // 7x7
	};

	return LookupTable[(uint8)Dictionary][(uint8)Size];
}

UTexture2D* AOpenCVCameraActor::TextureFromCvMat(cv::Mat& Mat)
{
	if ((Mat.cols <= 0) || (Mat.rows <= 0))
	{
		return nullptr;
	}

	// Only support CV_8U depth (8-bit unsigned)
	if (Mat.depth() != CV_8U)
	{
		return nullptr;
	}

	// Determine pixel format from channel count
	EPixelFormat PixelFormat;
	switch (Mat.channels())
	{
	case 1:
		PixelFormat = PF_G8;
		break;
	case 4:
		PixelFormat = PF_B8G8R8A8;
		break;
	default:
		return nullptr;
	}

	// Create the texture
	UTexture2D* NewTexture = UTexture2D::CreateTransient(Mat.cols, Mat.rows, PixelFormat);
	if (!NewTexture)
	{
		return nullptr;
	}

#if WITH_EDITORONLY_DATA
	NewTexture->MipGenSettings = TMGS_NoMipmaps;
#endif
	NewTexture->NeverStream = true;
	NewTexture->SRGB = 0;

	// Lock mip 0 and copy pixel data
	FTexture2DMipMap& Mip0 = NewTexture->GetPlatformData()->Mips[0];
	void* TextureData = Mip0.BulkData.Lock(LOCK_READ_WRITE);

	const int32 PixelStride = Mat.channels();
	FMemory::Memcpy(TextureData, Mat.data, Mat.cols * Mat.rows * SIZE_T(PixelStride));

	Mip0.BulkData.Unlock();
	NewTexture->UpdateResource();

	return NewTexture;
}

UTexture2D* AOpenCVCameraActor::TextureFromCvMat(cv::Mat& Mat, UTexture2D* InTexture)
{
	if (!InTexture)
	{
		return TextureFromCvMat(Mat);
	}
	
	if ((Mat.cols <= 0 || Mat.rows <= 0) || (Mat.depth() != CV_8U))
	{
		return nullptr;
	}
	
	EPixelFormat PixelFormat;
	switch (Mat.channels())
	{
	case 1:
		PixelFormat = PF_G8;
		break;
		
	case 2:
		PixelFormat = PF_B8G8R8A8;
		break;
		
	default:
		return nullptr;
	}
	if ((InTexture->GetSizeX() != Mat.cols) || (InTexture->GetSizeY() != Mat.rows) || (InTexture->GetPixelFormat() != PixelFormat))
	{
		return TextureFromCvMat(Mat);
	}
	
	// Copy the pixels from the OpenCV Mat to the Texture

	FTexture2DMipMap& Mip0 = InTexture->GetPlatformData()->Mips[0];
	void* TextureData = Mip0.BulkData.Lock(LOCK_READ_WRITE);

	const int32 PixelStride = Mat.channels();
	FMemory::Memcpy(TextureData, Mat.data, Mat.cols * Mat.rows * SIZE_T(PixelStride));

	Mip0.BulkData.Unlock();

	InTexture->UpdateResource();

	return InTexture;
}

void AOpenCVCameraActor::InitCameraTexture(int32 Width, int32 Height)
{
	// Create texture with Dynamic flag — designed for frequent CPU updates
	CameraTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!CameraTexture) return;
	
	CameraTexture->MipGenSettings = TMGS_NoMipmaps;
	CameraTexture->NeverStream = true;
	CameraTexture->SRGB = 0;
	CameraTexture->Filter = TF_Bilinear;
	CameraTexture->UpdateResource();

	if (DynMaterial)
	{
		DynMaterial->SetTextureParameterValue("CameraTexture", CameraTexture);
	}
}

bool AOpenCVCameraActor::DetectOnAllCandidates(const cv::Mat& Image)
{
	static const TCHAR* CandidateNames[] = {
		TEXT("Grayscale"),
		TEXT("Blue Channel"),
		TEXT("Green Channel"),
		TEXT("Red Channel"),
		TEXT("HSV Value"),
		TEXT("Otsu Binary"),
		TEXT("CLAHE"),
		TEXT("Inverted")
	};
	
	const int32 TotalCandidates = 8;

	for (int32 i = 0; i < TotalCandidates; i++)
	{
		cv::Mat Candidate = ExtractCandidate(Image, i);

		MarkerIds.clear();
		MarkerCorners.clear();
		RejectedCorners.clear();

		ArucoDetector.detectMarkers(
			Candidate, MarkerCorners, MarkerIds, RejectedCorners);
		
		/*
		UE_LOG(LogTemp, Warning,
			TEXT("[%s] Detected: %d | Rejected: %d"),
			CandidateNames[i],
			MarkerIds.size(),
			RejectedCorners.size());
			*/

		Candidate.release();

		if (MarkerIds.size() > 0)
		{
			// Store the winning index — DetectMarkers will use this from now on
			BestCandidateIndex = i;
			bCalibrated = true;

			UE_LOG(LogTemp, Warning,
				TEXT("Calibration complete — best channel: %s"),
				CandidateNames[i]);

			return true;
		}

		for (auto& M : MarkerCorners)  M.release();
		for (auto& M : RejectedCorners) M.release();
	}

	UE_LOG(LogTemp, Warning, TEXT("Calibration failed — no markers found"));
	return false;
}

cv::Mat AOpenCVCameraActor::ExtractCandidate(const cv::Mat& Image, int32 Index)
{
    cv::Mat BGR3;
    if (Image.channels() == 1)
    {
        cv::cvtColor(Image, BGR3, cv::COLOR_GRAY2BGR);
    }
    else if (Image.channels() == 4)
    {
        cv::cvtColor(Image, BGR3, cv::COLOR_BGRA2BGR);
    }
    else
    {
        BGR3 = Image;
    }

    // Log channel count once
    static bool bChannelLogged = false;
    if (!bChannelLogged)	
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Camera frame channels: %d"), Image.channels());
        bChannelLogged = true;
    }

    cv::Mat Candidate;
    cv::cvtColor(BGR3, Candidate, cv::COLOR_BGR2GRAY);

    switch (Index)
    {
    case 0:
        {
            BGR3.release();
            return Candidate;
        }
    case 1: case 2: case 3:
        {
    		cv::Mat Result;
    		cv::extractChannel(BGR3, Result, Index - 1);
    		BGR3.release();
    		return Result;
        }
    case 4:
        {
    		cv::Mat HSV;
    		cv::cvtColor(BGR3, HSV, cv::COLOR_BGR2HSV);
    		cv::Mat Result;
    		cv::extractChannel(HSV, Result, 2); // extract V channel directly
    		HSV.release();
    		Candidate.release();
    		BGR3.release();
    		return Result;	
        }
    case 5:
        {
            cv::Mat OtsuResult;
            cv::threshold(Candidate, OtsuResult, 0, 255,
                cv::THRESH_BINARY | cv::THRESH_OTSU);
            Candidate.release();
            BGR3.release();
            return OtsuResult;
        }
    case 6:
        {
    		cv::Mat Result;
    		Clahe->apply(Candidate, Result); // no more createCLAHE every frame
    		Candidate.release();
    		BGR3.release();
    		return Result;
        }
    case 7:
        {
            cv::Mat Result;
            cv::bitwise_not(Candidate, Result);
            Candidate.release();
            BGR3.release();
            return Result;
        }
    default:
        BGR3.release();
        return Candidate;
    }
}

void AOpenCVCameraActor::DetectMarkers(const cv::Mat& Image)
{
	// Blue channel gives best contrast for this marker set
	//std::vector<cv::Mat> Channels;
	//Channels.reserve(3);
	//cv::split(Image, Channels);

	selected_frame = ExtractCandidate(Image, BestCandidateIndex); // Blue channel (index 0 in BGR)

	//for (auto& M : Channels) M.release();

	MarkerIds.clear();
	MarkerCorners.clear();
	RejectedCorners.clear();

	ArucoDetector.detectMarkers(selected_frame, MarkerCorners, MarkerIds, RejectedCorners);

	if (MarkerIds.size() > 0)
	{
		for (int i = 0; i < (int)MarkerIds.size(); i++)
		{
			cv::Point2f* Corners = MarkerCorners[i].ptr<cv::Point2f>(0);

			std::vector<cv::Point> Polygon;
			for (int j = 0; j < 4; j++)
			{
				Polygon.push_back(cv::Point(
					(int)Corners[j].x,
					(int)Corners[j].y));
			}
			cv::polylines(Frame, Polygon, true, cv::Scalar(0, 255, 0), 3);

			//UE_LOG(LogTemp, Warning,TEXT("Marker ID: %d"), MarkerIds[i]);
		}
	}
}

void AOpenCVCameraActor::UpdateTextureFromMat(const cv::Mat& Image)
{
	if (!CameraTexture || Image.empty()) return;

	FTextureResource* TextureResource = CameraTexture->GetResource();
	if (!TextureResource) return;

	const int32 DataSize = Image.cols * Image.rows * 4;
	const int32 Width    = Image.cols;
	const int32 Height   = Image.rows;
	const int32 Pitch    = Image.cols * 4;

	TArray<uint8> PixelData;
	PixelData.SetNumUninitialized(DataSize);
	FMemory::Memcpy(PixelData.GetData(), Image.data, DataSize);

	ENQUEUE_RENDER_COMMAND(UpdateCameraTexture)(
		[TextureResource, PixelData = MoveTemp(PixelData), Width, Height, Pitch]
		(FRHICommandListImmediate& RHICmdList)
		{
			FUpdateTextureRegion2D Region(0, 0, 0, 0, Width, Height);

			// GetTextureRHI() returns FRHITexture* — no cast needed
			RHIUpdateTexture2D(
				TextureResource->GetTextureRHI(),
				0,
				Region,
				Pitch,
				PixelData.GetData()
			);
		});

}
