#include "Public/OpenCVCamera.h"
#include "Kismet/KismetSystemLibrary.h"
//#include "OpenCVHelper.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "opencv2/videoio/legacy/constants_c.h"

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
	
	THIRD_PARTY_INCLUDES_START
	Camera.open(0);
	if (!Camera.isOpened())
	{
		UKismetSystemLibrary::PrintString(this, TEXT("Unable to open camera"), true, true, FLinearColor::Red, 5.0f);
		return;
	}
	Camera.set(CV_CAP_PROP_FRAME_WIDTH, 1280);
	Camera.set(CV_CAP_PROP_FRAME_HEIGHT, 720);
	
	cv::aruco::Dictionary Dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_1000);
	cv::aruco::DetectorParameters Params;
	
	MarkerIds.reserve(100);
	MarkerCorners.reserve(100);
	RejectedCorners.reserve(100);
	
	ArucoDetector = cv::aruco::ArucoDetector(Dictionary, Params);
	
	UKismetSystemLibrary::PrintString(this, TEXT("Camera opened successfully"), true, true, FLinearColor::Green, 5.0f);
	THIRD_PARTY_INCLUDES_END
}	

void AOpenCVCameraActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	CameraTimer += DeltaTime;

	if (CameraTimer < CameraUpdateRate)
	{
		return;
	}

	CameraTimer = 0.0f;
	
THIRD_PARTY_INCLUDES_START

	MarkerIds.clear();
	MarkerCorners.clear();
	RejectedCorners.clear();
		
	Camera >> Frame;

	if (Frame.empty())
	{
		return;
	}
	
	cv::cvtColor(Frame, gray_frame, cv::COLOR_BGR2GRAY);

	//--------------------------------
	// ArUco Detection
	//--------------------------------
	
	ArucoDetector.detectMarkers(gray_frame, MarkerCorners, MarkerIds, RejectedCorners);

	//--------------------------------
	// Debug
	//--------------------------------

	if (MarkerIds.size() > 0)
	{
		for (int i = 0; i < (int)MarkerIds.size(); i++)
		{
			// Access corners via cv::Mat instead of inner vector
			cv::Point2f* Corners = MarkerCorners[i].ptr<cv::Point2f>(0);
        
			std::vector<cv::Point> Polygon;
			for (int j = 0; j < 4; j++)
			{
				Polygon.push_back(cv::Point((int)Corners[j].x, (int)Corners[j].y));
			}
			cv::polylines(Frame, Polygon, true, cv::Scalar(0, 255, 0), 3);
			cv::Point center((Corners[0].x + Corners[2].x) / 2,(Corners[0].y + Corners[2].y) / 2);
			cv::putText(Frame, std::to_string(MarkerIds[i]), center, cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(0, 0, 255), 5, cv::LINE_AA);
			
			UE_LOG(LogTemp, Warning,TEXT("Marker Detected: %d"),MarkerIds[i]);
		}
		cv::aruco::drawDetectedMarkers(gray_frame,MarkerCorners,MarkerIds);
		
	}
	
	cv::Mat BGRAFrame;
	cv::cvtColor(Frame,BGRAFrame,cv::COLOR_BGR2BGRA);
	CameraTexture = TextureFromCvMat(BGRAFrame, CameraTexture);
	
	// Check the texture VALID or NULL
	/* 
	if (CameraTexture)
	{
		UKismetSystemLibrary::PrintString(this,TEXT("Texture Valid"),true,true,FLinearColor::Blue,0.0f);
	}
	else
	{
		UKismetSystemLibrary::PrintString(this,TEXT("Texture NULL"),true,true,FLinearColor::Red,0.0f);
	}
	*/
	
	if (DynMaterial && CameraTexture)
	{
		DynMaterial->SetTextureParameterValue("CameraTexture",CameraTexture);
	}
THIRD_PARTY_INCLUDES_END
}

void AOpenCVCameraActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetActorTickEnabled(false);
	
	THIRD_PARTY_INCLUDES_START
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
	gray_frame.release();
	
	THIRD_PARTY_INCLUDES_END
		
	Super::EndPlay(EndPlayReason);
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
