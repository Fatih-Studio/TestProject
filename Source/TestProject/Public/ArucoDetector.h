#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

namespace cv
{
	class VideoCapture;

	namespace aruco
	{
		class ArucoDetector;
	}
}

#include "PreOpenCVLib.h"
#include "opencv2/opencv.hpp"
#include "opencv2/objdetect/aruco_detector.hpp"
#include "PostOpenCVLib.h"

#include "ArucoDetector.generated.h"

UCLASS()
class TESTPROJECT_API AArucoDetector : public AActor
{
	GENERATED_BODY()
	
public:
	AArucoDetector();
	
protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
public:
	virtual void Tick(float DeltaTime) override;
	
private:
	TUniquePtr<cv::VideoCapture> OpenCamera;
	TUniquePtr<cv::aruco::ArucoDetector> CameraDetector;
	std::vector<int> MarkerIds;
	std::vector<cv::Mat> MarkerCorners, RejectedCorners;
	
	bool bCameraOpened = false;
	
};
