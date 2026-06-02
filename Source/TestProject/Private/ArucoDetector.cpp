#include "ArucoDetector.h"


AArucoDetector::AArucoDetector()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AArucoDetector::BeginPlay()
{
	Super::BeginPlay();
	
	OpenCamera = MakeUnique<cv::VideoCapture>(0);
	
	if (OpenCamera && OpenCamera->isOpened())
	{
		bCameraOpened = true;
		UE_LOG(LogTemp, Log, TEXT("OpenCV Camera successfully initialized."));
		
		cv::aruco::Dictionary dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_1000);
		cv::aruco::DetectorParameters detectorParams;
		
		CameraDetector = MakeUnique<cv::aruco::ArucoDetector>(dictionary, detectorParams);
		
	}
	
}

void AArucoDetector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OpenCamera && OpenCamera->isOpened())
	{
		try
		{
			OpenCamera->release();
		}
		catch (...)
		{
		}
	}
	
	Super::EndPlay(EndPlayReason);
}

void AArucoDetector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (!bCameraOpened || !OpenCamera || !CameraDetector)
	{
		return;
	}

	cv::Mat Frame;
	*OpenCamera >> Frame;

	if (Frame.empty())
	{
		return;
	}

	// Storage for detection results

	// Detect markers in the current frame
	CameraDetector->detectMarkers(
		Frame,
		MarkerCorners,
		MarkerIds,
		RejectedCorners
	);

	// Process the results if any markers are found
	if (!MarkerIds.empty())
	{
		// Draw the markers onto the OpenCV cv::Mat matrix buffer
		cv::aruco::drawDetectedMarkers(Frame, MarkerCorners, MarkerIds);

		// Print IDs out to Unreal Engine's Log and Output Log window
		for (int Id : MarkerIds)
		{
			UE_LOG(LogTemp, Warning, TEXT("Detected Marker ID: %d"), Id);
		}
	}
}
