#include "Public/OpenCVCamera.h"
#include "Kismet/KismetSystemLibrary.h"
#include "OpenCVHelper.h"
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

#if WITH_OPENCV
	Camera.open(0);
	if (!Camera.isOpened())
	{
		UKismetSystemLibrary::PrintString(this, TEXT("Unable to open camera"), true, true, FLinearColor::Red, 5.0f);
		return;
	}
	//Camera.set(CV_CAP_PROP_FRAME_WIDTH, 1280);
	//Camera.set(CV_CAP_PROP_FRAME_HEIGHT, 720);
	
	UKismetSystemLibrary::PrintString(this, TEXT("Camera opened successfully"), true, true, FLinearColor::Green, 5.0f);
#endif
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
	
#if WITH_OPENCV

	Camera >> Frame;

	if (Frame.empty())
	{
		return;
	}
	
	cv::cvtColor(Frame, gray_frame, cv::COLOR_BGR2GRAY);

	//--------------------------------
	// ArUco Detection
	//--------------------------------

	std::vector<int> MarkerIds;
	std::vector<std::vector<cv::Point2f>> MarkerCorners, RejectedCorners;

	cv::Ptr<cv::aruco::Dictionary> Dictionary =cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_1000);

	cv::Ptr<cv::aruco::DetectorParameters> Params = cv::aruco::DetectorParameters::create();

	cv::aruco::detectMarkers(gray_frame,Dictionary,MarkerCorners,MarkerIds,Params, RejectedCorners);

	//--------------------------------
	// Debug
	//--------------------------------

	if (MarkerIds.size() > 0)
	{
		for (int i = 0; i < MarkerIds.size(); i++)
		{
			std::vector<cv::Point> Polygon;
			for (int j = 0; j < MarkerCorners[i].size(); j++)
			{
				Polygon.push_back(cv::Point(MarkerCorners[i][j].x, MarkerCorners[i][j].y));
			}
			cv::polylines(Frame, Polygon, true, cv::Scalar(0, 255, 0), 3);;
			
			UE_LOG(LogTemp, Warning,TEXT("Marker Detected: %d"),MarkerIds[i]);
		}
		//cv::aruco::drawDetectedMarkers(gray_frame,MarkerCorners,MarkerIds);
		
	}
	
	cv::Mat BGRAFrame;
	cv::cvtColor(Frame,BGRAFrame,cv::COLOR_BGR2BGRA);
	CameraTexture = FOpenCVHelper::TextureFromCvMat(BGRAFrame, CameraTexture);
	
	if (CameraTexture)
	{
		UKismetSystemLibrary::PrintString(this,TEXT("Texture Valid"),true,true,FLinearColor::Blue,0.0f);
	}
	else
	{
		UKismetSystemLibrary::PrintString(this,TEXT("Texture NULL"),true,true,FLinearColor::Red,0.0f);
	}
	
	if (DynMaterial && CameraTexture)
	{
		DynMaterial->SetTextureParameterValue("CameraTexture",CameraTexture);
	}
#endif
}

void AOpenCVCameraActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if WITH_OPENCV

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

#endif
	
	Super::EndPlay(EndPlayReason);
}
