#include "Tests/ConfiguratorTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ApartmentDataSubsystem.h"
#include "Camera/ConfiguratorCameraPawn.h"
#include "Core/ConfiguratorSceneProvider.h"
#include "Core/ConfiguratorStateSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace ConfiguratorTests
{
	UWorld* FindGameWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if ((Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) && Context.World())
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	APlayerController* GetPlayerController()
	{
		UWorld* World = FindGameWorld();
		return World ? World->GetFirstPlayerController() : nullptr;
	}

	UConfiguratorStateSubsystem* GetState()
	{
		UWorld* World = FindGameWorld();
		return World ? World->GetSubsystem<UConfiguratorStateSubsystem>() : nullptr;
	}

	UApartmentDataSubsystem* GetData()
	{
		UWorld* World = FindGameWorld();
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UApartmentDataSubsystem>() : nullptr;
	}

	AConfiguratorCameraPawn* GetCameraPawn()
	{
		APlayerController* Controller = GetPlayerController();
		return Controller ? Cast<AConfiguratorCameraPawn>(Controller->GetPawn()) : nullptr;
	}

	bool IsSceneReady()
	{
		const UApartmentDataSubsystem* Data = GetData();
		const UConfiguratorStateSubsystem* State = GetState();
		return Data && Data->HasData() && State && State->GetSceneProvider();
	}

	bool IsCameraIdle()
	{
		const AConfiguratorCameraPawn* Pawn = GetCameraPawn();
		return Pawn && Pawn->GetOrbitCamera() && !Pawn->GetOrbitCamera()->IsTransitioning();
	}

	float GetCameraDistance()
	{
		const AConfiguratorCameraPawn* Pawn = GetCameraPawn();
		return Pawn && Pawn->GetOrbitCamera() ? Pawn->GetOrbitCamera()->GetCurrentRig().Distance : -1.f;
	}

	float GetCameraYaw()
	{
		const AConfiguratorCameraPawn* Pawn = GetCameraPawn();
		return Pawn && Pawn->GetOrbitCamera() ? Pawn->GetOrbitCamera()->GetCurrentRig().Yaw : 0.f;
	}

	void RequestScreenshot(const FString& Name)
	{
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ScreenShotDir(), TEXT("Smoke"), Name + TEXT(".png")));
		FScreenshotRequest::RequestScreenshot(Path, /*bInShowUI*/ true, /*bAddUniqueSuffix*/ false);
	}

	void WaitUntil(FAutomationTestBase& Test, const TCHAR* What, TFunction<bool()> Condition)
	{
		FAutomationTestBase* TestPtr = &Test;
		TSharedRef<double> StartTime = MakeShared<double>(-1.0);
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([TestPtr, What, Condition, StartTime]() -> bool
		{
			const double Now = FPlatformTime::Seconds();
			if (*StartTime < 0.0)
			{
				*StartTime = Now;
			}
			if (Condition())
			{
				return true;
			}
			if (Now - *StartTime > StepTimeoutSeconds)
			{
				TestPtr->AddError(FString::Printf(TEXT("Timeout while waiting: %s"), What));
				return true;
			}
			return false;
		}));
	}

	void WaitSeconds(FAutomationTestBase& Test, double Seconds)
	{
		TSharedRef<double> EndTime = MakeShared<double>(-1.0);
		WaitUntil(Test, TEXT("delay"), [EndTime, Seconds]()
		{
			const double Now = FPlatformTime::Seconds();
			if (*EndTime < 0.0)
			{
				*EndTime = Now + Seconds;
			}
			return Now >= *EndTime;
		});
	}

	void Do(TFunction<void()> Action)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Action]() { Action(); return true; }));
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
