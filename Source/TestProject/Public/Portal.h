// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "Portal.generated.h"

class ALearnCPPCharacter;

UCLASS()
class TESTPROJECT_API APortal : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APortal();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* PortalMesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* PortalBoxCollision;
	
	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent*OverlappedComponent, AActor *OtherActor, 
						UPrimitiveComponent *OtherComp, int32 OtherBodyIndex, 
						bool bFromSweep, const FHitResult &SweepResult);
	
	UFUNCTION()
	void OnEndOverlap(UPrimitiveComponent*OverlappedComponent, AActor *OtherActor, 
						UPrimitiveComponent *OtherComp, int32 OtherBodyIndex);
	
	bool bIsTeleporting = false;
	
	FTimerHandle TeleportTimerHandle;
	float TeleportTime = 3.0f;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UPROPERTY(EditAnywhere, Category = "Portal")
	APortal* PortalRef;	

	UPROPERTY(BlueprintReadOnly,Category = "Portal")
	FTransform PortalRefTransform;
	
};
