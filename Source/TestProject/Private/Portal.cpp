// Fill out your copyright notice in the Description page of Project Settings.



#include "Public/Portal.h"
#include "Public/LearnCPPCharacter.h"
#include "Components/BoxComponent.h"
#include "Kismet/KismetSystemLibrary.h"

// Sets default values
APortal::APortal()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	
	PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMesh"));
	PortalMesh->SetupAttachment(RootComponent);
	
	PortalBoxCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollision"));
	PortalBoxCollision->SetupAttachment(PortalMesh);
	PortalBoxCollision->SetBoxExtent(FVector(500.0f, 500.0f, 200.0f));
	PortalBoxCollision->AddLocalOffset(FVector(0.0f, 0.0f, 200.0f));
	
	PortalBoxCollision->OnComponentBeginOverlap.AddDynamic(this, &APortal::OnBeginOverlap);
	PortalBoxCollision->OnComponentEndOverlap.AddDynamic(this, &APortal::OnEndOverlap);
	
}

// Called when the game starts or when spawned
void APortal::BeginPlay()
{
	Super::BeginPlay();
	
	// UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Portal '%s' BeginPlay!"), *GetActorLabel()), true, true, FLinearColor::Yellow, 5.0f);
	
	/* 
	 * if (PortalRef)
	 {
	 	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, *PortalRef->GetName());
	 } 
	 */
	//UKismetSystemLibrary::PrintString(this, TEXT("Poral Begin!"), true, true, FLinearColor::Yellow, 5.0f);
}

// Called every frame
void APortal::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	//UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Elapsed Time: '%f'"),GetWorld()->GetTimerManager().GetTimerElapsed(TeleportTimerHandle)), true, true, FLinearColor::Red, 1.0f);

}

void APortal::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor->IsA(ALearnCPPCharacter::StaticClass()))
	{
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Character entered portal: %s"),*GetActorLabel()), true, true, FLinearColor::Green, 5.0f);
		
		if (bIsTeleporting)
		{
			// UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Already teleporting through portal: %s"), *PortalRef->GetActorLabel()), true, true, FLinearColor::Yellow, 5.0f);
			return;
		}
		else
		{
			bIsTeleporting = true;
			 FString DebugMsg = FString::Printf(TEXT("Character entered portal: %s"), *GetActorLabel());
			 // UKismetSystemLibrary::PrintString(this, DebugMsg, true, true, FLinearColor::Green, 5.0f);
		}
			
		if (PortalRef && !this->bIsTeleporting)
		{
			this->bIsTeleporting = true;
			
			FString DebugMsg = FString::Printf (TEXT("Teleport to portal : %s"), *PortalRef->GetActorLabel());
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, DebugMsg);
			
			GetWorld()->GetTimerManager().SetTimer(TeleportTimerHandle, [this, OtherActor]()
			{
				if (this->bIsTeleporting)
				{
					FVector Location = PortalRef->GetActorLocation();
					OtherActor->TeleportTo(Location, OtherActor->GetActorRotation());
				}
				else
				{
					// GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Error: No PortalRef assigned in Details Panel!"));
					// UE_LOG(LogTemp, Error, TEXT("Portal '%s' failed to teleport because PortalRef is NULL!"), *GetActorLabel());
				}
			}, TeleportTime, false);
		}
		else if (!PortalRef)
		{
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Error: No PortalRef assigned in Details Panel!"));
			// UE_LOG(LogTemp, Error, TEXT("Portal '%s' failed to teleport because PortalRef is NULL!"), *GetActorLabel());
		}
	}
}

void APortal::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (OtherActor && OtherActor->IsA(ALearnCPPCharacter::StaticClass()))
	{
		// 1. Check if the timer is still waiting to fire
		if (GetWorld()->GetTimerManager().IsTimerActive(TeleportTimerHandle))
		{
			// The user actually left the zone BEFORE the time ran out
			GetWorld()->GetTimerManager().ClearTimer(TeleportTimerHandle);
			this->bIsTeleporting = false;

			FString DebugMsg = FString::Printf(TEXT("Teleport Cancelled, you left portal : %s"), *GetActorLabel());
			// UKismetSystemLibrary::PrintString(this, DebugMsg, true, true, FLinearColor::Red, 5.0f);
		}
		else
		{
			// The timer is NOT active, which means it either finished or was never started.
			// We only set bIsTeleporting to false here so we don't interrupt the timer logic.
			this->bIsTeleporting = false;

			// UE_LOG(LogTemp, Log, TEXT("Character left portal overlap area: %s"), *GetActorLabel());
		}
	}
}
	




