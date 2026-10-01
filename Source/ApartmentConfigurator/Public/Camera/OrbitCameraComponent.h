#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OrbitCameraComponent.generated.h"

/**
 * Положение орбитальной камеры: точка, вокруг которой она вращается, углы и дистанция.
 * Переходы интерполируют именно эти параметры, а не итоговую трансформацию, —
 * поэтому камера во время перелёта всегда смотрит на цель и не «срезает» углы.
 */
USTRUCT(BlueprintType)
struct APARTMENTCONFIGURATOR_API FOrbitCameraRig
{
	GENERATED_BODY()

	/** Точка фокуса (центр вращения), мировые координаты. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FVector Pivot = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float Yaw = 0.f;

	/** Отрицательный pitch — камера выше точки фокуса и смотрит вниз. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float Pitch = -30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "1.0"))
	float Distance = 3000.f;

	FRotator GetRotation() const { return FRotator(Pitch, Yaw, 0.f); }
	FVector GetCameraLocation() const { return Pivot - GetRotation().Vector() * Distance; }

	/** Интерполяция: yaw — по кратчайшей дуге, дистанция — в логарифмическом масштабе (равномерный визуальный зум). */
	static FOrbitCameraRig Interpolate(const FOrbitCameraRig& From, const FOrbitCameraRig& To, float Alpha);
};

/** Ограничения свободного управления камерой в текущем режиме. */
USTRUCT(BlueprintType)
struct APARTMENTCONFIGURATOR_API FOrbitCameraLimits
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MinDistance = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MaxDistance = 50000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MinPitch = -85.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MaxPitch = -3.f;

	void Clamp(FOrbitCameraRig& Rig) const;
};

/**
 * Орбитальная камера с плавными переходами.
 *
 * Отвечает только за математику движения: не знает о режимах, этажах и квартирах.
 * Какую позицию занять, решает владелец (AConfiguratorCameraPawn), здесь — как туда добраться.
 *
 * Производительность: тик компонента включается только на время перелёта или
 * сглаживания зума и выключается сразу по завершении. В состоянии покоя камера
 * не тратит ни одного вызова в кадр; вращение мышью применяется прямо в обработчике ввода.
 */
UCLASS(ClassGroup = (Configurator), meta = (BlueprintSpawnableComponent))
class APARTMENTCONFIGURATOR_API UOrbitCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOrbitCameraComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Компонент, которым управляет орбита (обычно UCameraComponent). */
	void SetControlledComponent(USceneComponent* InComponent);

	/**
	 * Плавный перелёт в новую позицию.
	 * @param Duration  длительность в секундах; <= 0 — вычисляется автоматически по длине пути.
	 */
	UFUNCTION(BlueprintCallable, Category = "Configurator|Camera")
	void TransitionTo(const FOrbitCameraRig& Target, const FOrbitCameraLimits& NewLimits, float Duration = -1.f);

	/** Мгновенная установка позиции (например, при старте). */
	UFUNCTION(BlueprintCallable, Category = "Configurator|Camera")
	void SnapTo(const FOrbitCameraRig& Target, const FOrbitCameraLimits& NewLimits);

	/** Вращение вокруг точки фокуса (градусы). Во время перелёта поворачивает и траекторию. */
	UFUNCTION(BlueprintCallable, Category = "Configurator|Camera")
	void AddOrbitInput(float DeltaYaw, float DeltaPitch);

	/** Зум колесом: положительное значение — приблизить. Сглаживается. */
	UFUNCTION(BlueprintCallable, Category = "Configurator|Camera")
	void AddZoomInput(float Steps);

	UFUNCTION(BlueprintPure, Category = "Configurator|Camera")
	bool IsTransitioning() const { return bTransitioning; }

	const FOrbitCameraRig& GetCurrentRig() const { return CurrentRig; }

	/** Длительность перелёта из текущей позиции в Target, если не задавать её явно. */
	float EstimateTransitionDuration(const FOrbitCameraRig& Target) const { return ComputeAutoDuration(CurrentRig, Target); }

	/** Минимальная длительность перелёта (короткие перемещения, например между соседними квартирами). */
	UPROPERTY(EditAnywhere, Category = "Transition", meta = (ClampMin = "0.05"))
	float MinTransitionDuration = 0.6f;

	/** Максимальная длительность перелёта (например, из общего вида к квартире). */
	UPROPERTY(EditAnywhere, Category = "Transition", meta = (ClampMin = "0.05"))
	float MaxTransitionDuration = 1.6f;

	/** Во сколько раз меняется дистанция за один щелчок колеса. */
	UPROPERTY(EditAnywhere, Category = "Zoom", meta = (ClampMin = "0.01", ClampMax = "0.5"))
	float ZoomStepFraction = 0.12f;

	/** Скорость сглаживания зума (больше — резче). */
	UPROPERTY(EditAnywhere, Category = "Zoom", meta = (ClampMin = "1.0"))
	float ZoomInterpSpeed = 10.f;

private:
	/** Плавный старт и остановка (smootherstep: нулевые скорость и ускорение на концах). */
	static float EaseInOut(float Alpha);

	float ComputeAutoDuration(const FOrbitCameraRig& From, const FOrbitCameraRig& To) const;

	void ApplyToControlledComponent() const;
	void UpdateTickEnabled();

	TWeakObjectPtr<USceneComponent> ControlledComponent;

	FOrbitCameraRig CurrentRig;
	FOrbitCameraRig TransitionFrom;
	FOrbitCameraRig TransitionTarget;
	FOrbitCameraLimits Limits;

	float TransitionElapsed = 0.f;
	float TransitionDuration = 1.f;
	bool bTransitioning = false;

	float ZoomGoalDistance = 0.f;
	bool bZooming = false;
};
