#include "Public/OpenCVCamera.h"

#include "opencv2/calib3d.hpp"

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
	
	if (LoadCalibration())
	{
		UKismetSystemLibrary::PrintString(this,
			FString::Printf(TEXT("Calibration auto-loaded — RMS: %.4f"), RMSError),true, true, FLinearColor::Green, 5.0f);
	}
	else
	{
		UKismetSystemLibrary::PrintString(this,
			TEXT("No calibration found — call StartCalibration()"),true, true, FLinearColor::Yellow, 5.0f);
	}
	
	Camera >> Frame;
	if (!Frame.empty())
	{
		FString Message = FString::Printf(TEXT("Cols = %d, Rows = %d"), Frame.cols, Frame.rows);
		UKismetSystemLibrary::PrintString(this, Message, true, true, FLinearColor::Green, 5.0f);
		InitCameraTexture(Frame.cols, Frame.rows);
	}

	// Fix exposure and focus to prevent hunting
	Camera.set(cv::CAP_PROP_AUTOFOCUS, 0);  // disable autofocus
	//Camera.set(cv::CAP_PROP_AUTO_EXPOSURE, 0); // disable auto exposure
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
	
	if (CalibrationState == ECalibrationState::Capturing)
	{
		CaptureCooldown += DeltaTime;

		if (CaptureCooldown >= CaptureCooldownTime)
		{
			// Convert to grayscale for chessboard detection
			cv::Mat Calib;
			cv::cvtColor(Frame, Calib, cv::COLOR_BGR2GRAY);

			if (TryCaptureCalibrationFrame(Calib))
			{
				CaptureCooldown = 0.0f; // reset cooldown after capture
			}

			Calib.release();
		}
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
		UE_LOG(LogTemp, Warning, TEXT("[%s] Detected: %d | Rejected: %d"), CandidateNames[i], MarkerIds.size(), RejectedCorners.size());
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
            TEXT("Camera frame channels: %d"), Image.channels()); bChannelLogged = true;
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

	selected_frame = ExtractCandidate(Image, BestCandidateIndex); 

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
			//cv::drawFrameAxes(Frame, CameraMatrix, DistCoeffs, rvecs[i], tvecs[i], 0.05);

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
			RHIUpdateTexture2D(TextureResource->GetTextureRHI(),0,Region,Pitch,PixelData.GetData());
		});

}


void AOpenCVCameraActor::StartCalibration()
{
	ResetCalibration();
	CalibrationState = ECalibrationState::Capturing;
	
	FString Messsage = FString::Printf(TEXT("Calibration started. Please show chessboard (%d, %d)"), ChessboardCornersX, ChessboardCornersY);
	UKismetSystemLibrary::PrintString(this, Messsage, true, true, FLinearColor::Yellow, 5.0f);
}

void AOpenCVCameraActor::ResetCalibration()
{
	CalibObjPoints.clear();
	CalibImgPoints.clear();
	CapturedFrames   = 0;
	RMSError         = 0.0f;
	CaptureCooldown  = 0.0f;
	CalibrationState = ECalibrationState::Idle;
}

bool AOpenCVCameraActor::LoadCalibration()
{
	FString LoadPath = FPaths::ProjectSavedDir() / TEXT("CameraCalibration.json");
	FString Json;

	if (!FFileHelper::LoadFileToString(Json, *LoadPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("No calibration file found at: %s"), *LoadPath);
		return false;
	}

	// Simple key extraction without full JSON parser
	auto ExtractValue = [&](const FString& Key) -> float
	{
		FString Search = FString::Printf(TEXT("\"%s\""), *Key);
		int32 Idx = Json.Find(Search);
		if (Idx == INDEX_NONE) return 0.0f;

		int32 ColonIdx = Json.Find(TEXT(":"), ESearchCase::IgnoreCase,
			ESearchDir::FromStart, Idx);
		if (ColonIdx == INDEX_NONE) return 0.0f;

		FString Remainder = Json.Mid(ColonIdx + 1).TrimStartAndEnd();
		return FCString::Atof(*Remainder);
	};

	RMSError     = ExtractValue(TEXT("rms"));
	FocalLengthX = ExtractValue(TEXT("fx"));
	FocalLengthY = ExtractValue(TEXT("fy"));
	PrincipalX   = ExtractValue(TEXT("cx"));
	PrincipalY   = ExtractValue(TEXT("cy"));
	DistK1       = ExtractValue(TEXT("k1"));
	DistK2       = ExtractValue(TEXT("k2"));
	DistP1       = ExtractValue(TEXT("p1"));
	DistP2       = ExtractValue(TEXT("p2"));

	CalibrationState   = ECalibrationState::Calibrated;
	bCalibrationLoaded = true;

	UE_LOG(LogTemp, Warning,
		TEXT("Calibration loaded — RMS: %.4f fx: %.2f fy: %.2f"),
		RMSError, FocalLengthX, FocalLengthY);

	return true;
}


bool AOpenCVCameraActor::TryCaptureCalibrationFrame(const cv::Mat& CalibFrame)
{
	cv::Size BoardSize(ChessboardCornersX, ChessboardCornersY);
	std::vector<cv::Point2f> Corners;
	
	bool bFound	= cv::findChessboardCorners(CalibFrame, BoardSize, Corners, cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_FAST_CHECK);
	
	if (!bFound) return false;
	
	// Refine to subpixel accuracy
	cv::TermCriteria Criteria(
		cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.001);
	cv::cornerSubPix(CalibFrame, Corners, cv::Size(11, 11), cv::Size(-1, -1), Criteria);

	// Draw detected corners on Frame for visual feedback
	cv::Mat DisplayFrame;
	cv::cvtColor(CalibFrame, DisplayFrame, cv::COLOR_GRAY2BGR);
	cv::drawChessboardCorners(DisplayFrame, BoardSize, Corners, bFound);

	// Overlay on main frame
	cv::addWeighted(CalibFrame, 0.7, DisplayFrame, 0.3, 0, Frame);
	DisplayFrame.release();

	// Check if frame is diverse enough from existing captures
	if (!IsFrameDiverse(Corners)) return false;

	// Build 3D object points for this board size
	std::vector<cv::Point3f> ObjPoints;
	for (int y = 0; y < ChessboardCornersY; y++)
		for (int x = 0; x < ChessboardCornersX; x++)
			ObjPoints.push_back(cv::Point3f(x * SquareSizeMM, y * SquareSizeMM, 0.0f));

	CalibObjPoints.push_back(ObjPoints);
	CalibImgPoints.push_back(Corners);
	CalibImageSize = CalibFrame.size();
	CapturedFrames++;
	
	FString Messsage = FString::Printf(TEXT("Captured calibration frame %d/%d"), CapturedFrames, RequiredFrames);
	UKismetSystemLibrary::PrintString(this, Messsage, true, true, FLinearColor::Green, 1.0f);
	
	// Auto-run calibration when enough frames collected
	if (CapturedFrames >= RequiredFrames)
	{
		CalibrationState = ECalibrationState::Calibrating;
		RunCalibration();
	}

	return true;
}

bool AOpenCVCameraActor::IsFrameDiverse(const std::vector<cv::Point2f>& NewCorners)
{
	if (CalibImgPoints.empty()) return true;

	// Compute mean position of new corners
	cv::Point2f NewMean(0.0f, 0.0f);
	for (auto& P : NewCorners) NewMean += P;
	NewMean.x /= NewCorners.size();
	NewMean.y /= NewCorners.size();

	// Compare against all existing captured frames
	for (auto& Existing : CalibImgPoints)
	{
		cv::Point2f ExistMean(0.0f, 0.0f);
		for (auto& P : Existing) ExistMean += P;
		ExistMean.x /= Existing.size();
		ExistMean.y /= Existing.size();

		float Dx   = NewMean.x - ExistMean.x;
		float Dy   = NewMean.y - ExistMean.y;
		float Dist = FMath::Sqrt(Dx * Dx + Dy * Dy);

		if (Dist < 40.0f) // pixels — too close to an existing frame
		{
			return false;
		}
	}

	return true;
}

bool AOpenCVCameraActor::RunCalibration()
{
	UKismetSystemLibrary::PrintString(this, TEXT("Running calibration — please wait..."), true, true, FLinearColor::Yellow, 3.0f);

	cv::Mat CameraMatrix, DistCoeffs;
	std::vector<cv::Mat> RVecs, TVecs;

	double RMS = cv::calibrateCamera(CalibObjPoints,CalibImgPoints,CalibImageSize,CameraMatrix,DistCoeffs,RVecs, TVecs);

	RMSError = (float)RMS;

	if (RMS > 3.0)
	{
		CalibrationState = ECalibrationState::Failed;
		UKismetSystemLibrary::PrintString(this,FString::Printf(TEXT("Calibration FAILED — RMS too high: %.2f px. Recapture with better images."), RMS),true, true, FLinearColor::Red, 10.0f);
		return false;
	}

	// Store results into UPROPERTY variables
	FocalLengthX = (float)CameraMatrix.at<double>(0, 0);
	FocalLengthY = (float)CameraMatrix.at<double>(1, 1);
	PrincipalX   = (float)CameraMatrix.at<double>(0, 2);
	PrincipalY   = (float)CameraMatrix.at<double>(1, 2);

	if (DistCoeffs.total() >= 4)
	{
		DistK1 = (float)DistCoeffs.at<double>(0);
		DistK2 = (float)DistCoeffs.at<double>(1);
		DistP1 = (float)DistCoeffs.at<double>(2);
		DistP2 = (float)DistCoeffs.at<double>(3);
	}

	CalibrationState  = ECalibrationState::Calibrated;
	bCalibrationLoaded = true;

	UKismetSystemLibrary::PrintString(this,
		FString::Printf(TEXT("Calibration SUCCESS — RMS: %.4f px\nfx=%.2f fy=%.2f cx=%.2f cy=%.2f"),RMS, FocalLengthX, FocalLengthY, PrincipalX, PrincipalY),true, true, FLinearColor::Green, 10.0f);

	SaveCalibration();
	return true;
}

void AOpenCVCameraActor::SaveCalibration()
{
	FString SavePath = FPaths::ProjectSavedDir() / TEXT("CameraCalibration.json");

	FString Json = FString::Printf(
		TEXT("{\n")
		TEXT("  \"rms\"  : %.6f,\n")
		TEXT("  \"fx\"   : %.6f,\n")
		TEXT("  \"fy\"   : %.6f,\n")
		TEXT("  \"cx\"   : %.6f,\n")
		TEXT("  \"cy\"   : %.6f,\n")
		TEXT("  \"k1\"   : %.6f,\n")
		TEXT("  \"k2\"   : %.6f,\n")
		TEXT("  \"p1\"   : %.6f,\n")
		TEXT("  \"p2\"   : %.6f\n")
		TEXT("}"),
		RMSError,
		FocalLengthX, FocalLengthY,
		PrincipalX, PrincipalY,
		DistK1, DistK2, DistP1, DistP2);

	if (FFileHelper::SaveStringToFile(Json, *SavePath))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Calibration saved to: %s"), *SavePath);
	}
}
