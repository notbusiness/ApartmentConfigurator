#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

class AConfiguratorCameraPawn;
class APlayerController;
class FAutomationTestBase;
class UApartmentDataSubsystem;
class UConfiguratorStateSubsystem;
class UWorld;

/**
 * Общие помощники игровых автотестов (Configurator.Game.*): доступ к миру и подсистемам
 * запущенной игры и латентные шаги сценария (ожидание условия с таймаутом, пауза, действие).
 */
namespace ConfiguratorTests
{
	/** Предельное время ожидания любого шага (загрузка данных, перелёт камеры, реакция на ввод). */
	constexpr double StepTimeoutSeconds = 30.0;

	UWorld* FindGameWorld();
	APlayerController* GetPlayerController();
	UConfiguratorStateSubsystem* GetState();
	UApartmentDataSubsystem* GetData();
	AConfiguratorCameraPawn* GetCameraPawn();

	bool IsSceneReady();
	bool IsCameraIdle();
	float GetCameraDistance();
	float GetCameraYaw();

	/** Скриншот с UI в Saved/Screenshots/<платформа>/Smoke/<Name>.png. */
	void RequestScreenshot(const FString& Name);

	/** Латентный шаг: ждать, пока Condition не станет true (при таймауте — ошибка теста). */
	void WaitUntil(FAutomationTestBase& Test, const TCHAR* What, TFunction<bool()> Condition);

	/** Латентный шаг: пауза, отсчитываемая с момента начала выполнения шага. */
	void WaitSeconds(FAutomationTestBase& Test, double Seconds);

	/** Латентный шаг: выполнить действие один раз. */
	void Do(TFunction<void()> Action);
}

#endif // WITH_DEV_AUTOMATION_TESTS
