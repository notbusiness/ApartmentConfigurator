#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Camera/OrbitCameraComponent.h"
#include "Core/ConfiguratorViewTypes.h"
#include "ConfiguratorCameraPawn.generated.h"

class UCameraComponent;
class UConfiguratorStateSubsystem;

/** Параметры ракурса для одного режима камеры. */
USTRUCT(BlueprintType)
struct APARTMENTCONFIGURATOR_API FConfiguratorShotSettings
{
	GENERATED_BODY()

	/** Смещение yaw относительно фасада (0 — строго фронтально). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	float YawOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	float Pitch = -20.f;

	/** Запас кадра: 1.0 — объект ровно вписан в кадр (по всем 8 углам габарита), больше — с полями. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (ClampMin = "0.5"))
	float FramingMargin = 1.15f;

	/** Нижняя граница дистанции для этого ракурса (защита от слишком маленьких объектов). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (ClampMin = "10.0"))
	float MinShotDistance = 500.f;

	/** Свободный зум относительно дистанции ракурса: [Base * MinZoom, Base * MaxZoom]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (ClampMin = "0.1"))
	float MinZoom = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (ClampMin = "0.1"))
	float MaxZoom = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	float MinPitch = -80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	float MaxPitch = -3.f;
};

/**
 * Пешка-камера конфигуратора.
 *
 * Здесь живёт логика «режим -> ракурс»: пешка подписана на UConfiguratorStateSubsystem
 * и при смене вида вычисляет целевую позицию орбиты по геометрии из IConfiguratorSceneProvider.
 * Само движение выполняет UOrbitCameraComponent. UI о камере ничего не знает.
 *
 * Почему пешка, а не CameraActor + менеджер: пешка естественно получает ввод от контроллера
 * и становится view target без дополнительной настройки PlayerCameraManager.
 */
UCLASS()
class APARTMENTCONFIGURATOR_API AConfiguratorCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AConfiguratorCameraPawn();

	/** Вращение (градусы) — вызывается контроллером при перетаскивании мыши. */
	void AddOrbitInput(float DeltaYaw, float DeltaPitch);

	/** Зум колесом мыши. */
	void AddZoomInput(float Steps);

	UOrbitCameraComponent* GetOrbitCamera() const { return OrbitCamera; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleViewChanged(const FConfiguratorViewState& NewView, EConfiguratorViewChangeReason Reason);

	/** Вычисляет ракурс для вида. false — геометрия ещё не готова. */
	bool ComputeRigForView(const FConfiguratorViewState& View, FOrbitCameraRig& OutRig, FOrbitCameraLimits& OutLimits) const;

	/**
	 * Минимальная дистанция от точки фокуса, на которой бокс целиком виден при данном повороте камеры.
	 * Margin > 1 — запас по краям кадра (1.1 = 10%).
	 */
	float ComputeFitDistance(const FBox& Bounds, const FVector& Pivot, const FRotator& ViewRotation, float Margin) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UOrbitCameraComponent> OrbitCamera;

	UPROPERTY(EditAnywhere, Category = "Shots")
	FConfiguratorShotSettings GenplanShot;

	UPROPERTY(EditAnywhere, Category = "Shots")
	FConfiguratorShotSettings FloorShot;

	UPROPERTY(EditAnywhere, Category = "Shots")
	FConfiguratorShotSettings ApartmentShot;

	/** Возврат кнопкой «Назад» чуть быстрее прямого перехода — так он ощущается отзывчивее. */
	UPROPERTY(EditAnywhere, Category = "Shots", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float BackTransitionSpeedScale = 0.8f;

private:
	TWeakObjectPtr<UConfiguratorStateSubsystem> StateSubsystem;
};
