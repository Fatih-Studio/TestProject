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

// Calibration State
UENUM(BlueprintType)
enum class ECalibrationState : uint8
{
	Idle        UMETA(DisplayName = "Idle"),
	Capturing   UMETA(DisplayName = "Capturing Frames"),
	Calibrating UMETA(DisplayName = "Calibrating"),
	Calibrated  UMETA(DisplayName = "Calibrated"),
	Failed      UMETA(DisplayName = "Failed")
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
	
	// Calibration Variable
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float FocalLengthX = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float FocalLengthY = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float PrincipalX = 0.0f;   // cx — usually image width / 2

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float PrincipalY = 0.0f;   // cy — usually image height / 2

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float DistK1 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float DistK2 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float DistP1 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float DistP2 = 0.0f;
	
	// Calibration Setting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	int32 ChessboardCornersX = 9;   // inner corners horizontal

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	int32 ChessboardCornersY = 6;   // inner corners vertical

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float SquareSizeMM = 25.0f;     // physical square size in mm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	int32 RequiredFrames = 20;      // frames needed before calibrating
	
	// Calibration Status
	UPROPERTY(BlueprintReadOnly, Category = "Calibration")
	ECalibrationState CalibrationState = ECalibrationState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Calibration")
	int32 CapturedFrames = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Calibration")
	float RMSError = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Calibration")
	bool bCalibrationLoaded = false;
	
	// Calibration Control
	UFUNCTION(BlueprintCallable, Category = "Calibration")
	void StartCalibration();

	UFUNCTION(BlueprintCallable, Category = "Calibration")
	void ResetCalibration();

	UFUNCTION(BlueprintCallable, Category = "Calibration")
	bool LoadCalibration();	
	
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
	
	// Calibration
	// Internal calibration data
	std::vector<std::vector<cv::Point3f>> CalibObjPoints;
	std::vector<std::vector<cv::Point2f>> CalibImgPoints;
	cv::Size CalibImageSize;

	float CaptureCooldown     = 0.0f;
	float CaptureCooldownTime = 1.5f;  // seconds between auto-captures

	bool TryCaptureCalibrationFrame(const cv::Mat& Frame);
	bool IsFrameDiverse(const std::vector<cv::Point2f>& NewCorners);
	bool RunCalibration();
	void SaveCalibration();
};
