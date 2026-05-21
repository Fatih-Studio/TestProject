// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestProject.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"





IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, TestProject, "TestProject" );

DEFINE_LOG_CATEGORY(LogTestProject)

void FTestProjectModule::StartupModule()
{
#if PLATFORM_WINDOWS
	FString DLLPath = FPaths::ProjectDir() / TEXT("Binaries/Win64/opencv_world4120.dll");
	FPaths::MakeStandardFilename(DLLPath);

	OpenCVDLLHandle = FPlatformProcess::GetDllHandle(*DLLPath);

	if (!OpenCVDLLHandle)
	{
		UE_LOG(LogTemp, Fatal, TEXT("Failed to load opencv_world4120.dll from: %s"), *DLLPath);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("OpenCV DLL loaded successfully"));
	}
#endif
	
	IModuleInterface::StartupModule();
}

void FTestProjectModule::ShutdownModule()
{
	IModuleInterface::ShutdownModule();
	
#if PLATFORM_WINDOWS
	if (OpenCVDLLHandle)
	{
		FPlatformProcess::FreeDllHandle(OpenCVDLLHandle);
		OpenCVDLLHandle = nullptr;
	}
#endif
}

