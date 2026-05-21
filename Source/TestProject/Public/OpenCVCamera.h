#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"

THIRD_PARTY_INCLUDES_START
#include "PreOpenCVHeaders.h"
#include "opencv2/highgui.hpp"
#include "opencv2/core/utility.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/objdetect/aruco_detector.hpp"
#include "opencv2/videoio/videoio.hpp"
#include "opencv2/core/mat.hpp"
#include "opencv2/objdetect/aruco_dictionary.hpp"
#include "PostOpenCVHeaders.h"
THIRD_PARTY_INCLUDES_END

#include "OpenCVCamera.generated.h"

class UTexture2D;

UCLASS()
class AOpenCVCameraActor : public AActor
{
	GENERATED_BODY()

public:
	AOpenCVCameraActor();
	
protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
public:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* PlaneMesh;
	
	UPROPERTY(BlueprintReadOnly)
	UTexture2D* CameraTexture;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* BaseMaterial;
	
	UMaterialInstanceDynamic* DynMaterial;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraUpdateRate = 1.0f / 30.0f;

	float CameraTimer = 0.0f;
	
	static UTexture2D* TextureFromCvMat(cv::Mat& Mat);
	
	static UTexture2D* TextureFromCvMat(cv::Mat& Mat, UTexture2D* InTexture);
	
private:
	THIRD_PARTY_INCLUDES_START
		cv::VideoCapture Camera;
		cv::Mat Frame;
		cv::Mat gray_frame;
		cv::aruco::ArucoDetector ArucoDetector;
	
		std::vector<int> MarkerIds;
		std::vector<cv::Mat> MarkerCorners, RejectedCorners;
	THIRD_PARTY_INCLUDES_END
};
