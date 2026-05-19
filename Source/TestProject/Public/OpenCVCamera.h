#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "OpenCVCamera.generated.h"

#if WITH_OPENCV
#include "PreOpenCVHeaders.h"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/aruco.hpp>
#include "PostOpenCVHeaders.h"
#endif

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

	float CameraUpdateRate = 1.0f / 30.0f;

	float CameraTimer = 0.0f;
	
private:
#if WITH_OPENCV
	cv::VideoCapture Camera;
	cv::Mat Frame;
	cv::Mat gray_frame;
#endif
};
