#include "Camera/OrbitCameraComponent.h"

#include "Components/SceneComponent.h"

FOrbitCameraRig FOrbitCameraRig::Interpolate(const FOrbitCameraRig& From, const FOrbitCameraRig& To, float Alpha)
{
	FOrbitCameraRig Result;
	Result.Pivot = FMath::Lerp(From.Pivot, To.Pivot, static_cast<double>(Alpha));
	Result.Yaw = From.Yaw + FMath::FindDeltaAngleDegrees(From.Yaw, To.Yaw) * Alpha;
	Result.Pitch = FMath::Lerp(From.Pitch, To.Pitch, Alpha);

	// Линейная интерполяция дистанции даёт «рывок» в конце приближения:
	// последние метры проходятся с той же скоростью, что и первые километры.
	// В логарифмическом масштабе относительная скорость зума постоянна.
	const float LogFrom = FMath::Loge(FMath::Max(From.Distance, 1.f));
	const float LogTo = FMath::Loge(FMath::Max(To.Distance, 1.f));
	Result.Distance = FMath::Exp(FMath::Lerp(LogFrom, LogTo, Alpha));
	return Result;
}

void FOrbitCameraLimits::Clamp(FOrbitCameraRig& Rig) const
{
	Rig.Distance = FMath::Clamp(Rig.Distance, MinDistance, FMath::Max(MinDistance, MaxDistance));
	Rig.Pitch = FMath::Clamp(Rig.Pitch, MinPitch, FMath::Max(MinPitch, MaxPitch));
	Rig.Yaw = FMath::UnwindDegrees(Rig.Yaw);
}

UOrbitCameraComponent::UOrbitCameraComponent()
{
	// Тик нужен только во время анимации — по умолчанию выключен.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UOrbitCameraComponent::SetControlledComponent(USceneComponent* InComponent)
{
	ControlledComponent = InComponent;
	ApplyToControlledComponent();
}

void UOrbitCameraComponent::TransitionTo(const FOrbitCameraRig& Target, const FOrbitCameraLimits& NewLimits, float Duration)
{
	Limits = NewLimits;

	TransitionFrom = CurrentRig;
	TransitionTarget = Target;
	Limits.Clamp(TransitionTarget);

	TransitionDuration = Duration > 0.f ? Duration : ComputeAutoDuration(TransitionFrom, TransitionTarget);
	TransitionElapsed = 0.f;
	bTransitioning = true;

	// Незавершённый зум поглощается перелётом, иначе он «перетянет» дистанцию после прибытия.
	bZooming = false;

	UpdateTickEnabled();
}

void UOrbitCameraComponent::SnapTo(const FOrbitCameraRig& Target, const FOrbitCameraLimits& NewLimits)
{
	Limits = NewLimits;
	CurrentRig = Target;
	Limits.Clamp(CurrentRig);

	bTransitioning = false;
	bZooming = false;
	UpdateTickEnabled();
	ApplyToControlledComponent();
}

void UOrbitCameraComponent::AddOrbitInput(float DeltaYaw, float DeltaPitch)
{
	if (FMath::IsNearlyZero(DeltaYaw) && FMath::IsNearlyZero(DeltaPitch))
	{
		return;
	}

	if (bTransitioning)
	{
		// Поворачиваем всю траекторию: перелёт продолжается, но уже с учётом ввода пользователя.
		TransitionFrom.Yaw += DeltaYaw;
		TransitionFrom.Pitch += DeltaPitch;
		TransitionTarget.Yaw += DeltaYaw;
		TransitionTarget.Pitch += DeltaPitch;
		Limits.Clamp(TransitionFrom);
		Limits.Clamp(TransitionTarget);
		return;
	}

	// В покое — прямое применение без тика: вращение мышью — это прямое манипулирование,
	// дополнительное сглаживание здесь ощущалось бы как задержка.
	CurrentRig.Yaw += DeltaYaw;
	CurrentRig.Pitch += DeltaPitch;
	Limits.Clamp(CurrentRig);
	ApplyToControlledComponent();
}

void UOrbitCameraComponent::AddZoomInput(float Steps)
{
	if (FMath::IsNearlyZero(Steps))
	{
		return;
	}

	const float Factor = FMath::Pow(1.f - ZoomStepFraction, Steps);

	if (bTransitioning)
	{
		TransitionTarget.Distance *= Factor;
		Limits.Clamp(TransitionTarget);
		return;
	}

	const float Base = bZooming ? ZoomGoalDistance : CurrentRig.Distance;
	ZoomGoalDistance = FMath::Clamp(Base * Factor, Limits.MinDistance, FMath::Max(Limits.MinDistance, Limits.MaxDistance));
	bZooming = true;
	UpdateTickEnabled();
}

void UOrbitCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bTransitioning)
	{
		TransitionElapsed += DeltaTime;
		const float Alpha = FMath::Clamp(TransitionElapsed / FMath::Max(TransitionDuration, UE_KINDA_SMALL_NUMBER), 0.f, 1.f);
		CurrentRig = FOrbitCameraRig::Interpolate(TransitionFrom, TransitionTarget, EaseInOut(Alpha));

		if (Alpha >= 1.f)
		{
			CurrentRig = TransitionTarget;
			bTransitioning = false;
		}
	}
	else if (bZooming)
	{
		CurrentRig.Distance = FMath::FInterpTo(CurrentRig.Distance, ZoomGoalDistance, DeltaTime, ZoomInterpSpeed);
		if (FMath::IsNearlyEqual(CurrentRig.Distance, ZoomGoalDistance, 0.5f))
		{
			CurrentRig.Distance = ZoomGoalDistance;
			bZooming = false;
		}
	}

	ApplyToControlledComponent();
	UpdateTickEnabled();
}

float UOrbitCameraComponent::EaseInOut(float Alpha)
{
	return Alpha * Alpha * Alpha * (Alpha * (Alpha * 6.f - 15.f) + 10.f);
}

float UOrbitCameraComponent::ComputeAutoDuration(const FOrbitCameraRig& From, const FOrbitCameraRig& To) const
{
	// Условная «длина пути»: смещение точки фокуса относительно масштаба сцены,
	// изменение масштаба (зум) и поворот. Короткие перемещения — быстрые, дальние — дольше, но не бесконечно.
	const float Scale = FMath::Max(FMath::Min(From.Distance, To.Distance), 100.f);
	const float PivotTravel = static_cast<float>(FVector::Dist(From.Pivot, To.Pivot)) / Scale;
	const float ZoomTravel = FMath::Abs(FMath::Loge(FMath::Max(To.Distance, 1.f) / FMath::Max(From.Distance, 1.f)));
	const float TurnTravel = FMath::Abs(FMath::FindDeltaAngleDegrees(From.Yaw, To.Yaw)) / 180.f;

	const float PathLength = PivotTravel + ZoomTravel + TurnTravel;
	return FMath::Clamp(MinTransitionDuration + 0.35f * PathLength, MinTransitionDuration, MaxTransitionDuration);
}

void UOrbitCameraComponent::ApplyToControlledComponent() const
{
	if (USceneComponent* Target = ControlledComponent.Get())
	{
		Target->SetWorldLocationAndRotation(CurrentRig.GetCameraLocation(), CurrentRig.GetRotation());
	}
}

void UOrbitCameraComponent::UpdateTickEnabled()
{
	const bool bNeedsTick = bTransitioning || bZooming;
	if (IsComponentTickEnabled() != bNeedsTick)
	{
		SetComponentTickEnabled(bNeedsTick);
	}
}
