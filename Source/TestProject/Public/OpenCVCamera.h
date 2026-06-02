#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "TextureResource.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "Kismet/KismetSystemLibrary.h"

#include "PreOpenCVLib.h"	
#include "opencv2/highgui.hpp"
#include "opencv2/core/utility.hpp"
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/objdetect/aruco_detector.hpp"
#include "opencv2/videoio/videoio.hpp"
#include "opencv2/videoio/legacy/constants_c.h"
#include "opencv2/core/mat.hpp"
#include "opencv2/objdetect/aruco_dictionary.hpp"
#include "PostOpenCVLib.h"

#include "OpenCVCamera.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EOpenCVArucoDictionary : uint8
{
	Dict4x4 UMETA(DisplayName = "4x4"),
	Dict5x5 UMETA(DisplayName = "5x5"),
	Dict6x6 UMETA(DisplayName = "6x6"),
	Dict7x7 UMETA(DisplayName = "7x7"),
	DictOriginal UMETA(DisplayName = "Original")
};

UENUM(BlueprintType)
enum class EOpenCVArucoDictionarySize : uint8
{
	DictSize50 UMETA(DisplayName = "50"),
	DictSize100 UMETA(DisplayName = "100"),
	DictSize250 UMETA(DisplayName = "250"),
	DictSize1000 UMETA(DisplayName = "1000")
};

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
	
	static cv::aruco::PredefinedDictionaryType GetOpenCVDictionaryType(EOpenCVArucoDictionary Dictionary, EOpenCVArucoDictionarySize Size);
	
public:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* PlaneMesh;
	
	UPROPERTY(BlueprintReadOnly)
	UTexture2D* CameraTexture;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* BaseMaterial;
	
	UMaterialInstanceDynamic* DynMaterial;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aruco", meta = (ToolTip = "How often update for the detection (in frame per seconds)"))
	float ArucoUpdateRate = 30.0f;

	float ArucoTimer = 0.0f;
	
	static UTexture2D* TextureFromCvMat(cv::Mat& Mat);
	
	static UTexture2D* TextureFromCvMat(cv::Mat& Mat, UTexture2D* InTexture);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aruco")
	EOpenCVArucoDictionary DictionaryGrid = EOpenCVArucoDictionary::Dict6x6;

	// Hidden when DictOriginal is selected — it has no size
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aruco", meta = (EditCondition = "DictionaryGrid != EOpenCVArucoDictionary::DictOriginal"))
	EOpenCVArucoDictionarySize DictionarySize = EOpenCVArucoDictionarySize::DictSize1000;
	
	void InitCameraTexture(int32 Width, int32 Height);
	
private:
	cv::VideoCapture Camera;
	cv::Mat Frame;
	cv::Mat selected_frame;
	cv::aruco::ArucoDetector ArucoDetector;
	
	std::vector<int> MarkerIds;
	std::vector<cv::Mat> MarkerCorners, RejectedCorners;
	
	bool DetectOnAllCandidates(const cv::Mat& Image);
	int32 BestCandidateIndex = -1; // -1 = not calibrated yet
	cv::Ptr<cv::CLAHE> Clahe;
	bool bCalibrated = false;
	int32 FramesWithNoDetection = 0;
	static const int32 RecalibrationThreshold = 60;
	cv::Mat ExtractCandidate(const cv::Mat& Image, int32 Index);
	void DetectMarkers(const cv::Mat& Image);
	void UpdateTextureFromMat(const cv::Mat& Image);
};
