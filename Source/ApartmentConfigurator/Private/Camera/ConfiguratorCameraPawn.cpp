#include "Camera/ConfiguratorCameraPawn.h"

#include "ApartmentConfigurator.h"
#include "Camera/CameraComponent.h"
#include "Core/ConfiguratorSceneProvider.h"
#include "Core/ConfiguratorStateSubsystem.h"
#include "Engine/World.h"

namespace
{
	/** Соотношение сторон, под которое вписываются объекты (реальное окно может быть шире — тогда просто больше запаса). */
	constexpr float ReferenceAspectRatio = 16.f / 9.f;
}

AConfiguratorCameraPawn::AConfiguratorCameraPawn()
{
	// Пешке не нужен собственный тик: всё движение — в UOrbitCameraComponent и только во время анимации.
	PrimaryActorTick.bCanEverTick = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SceneRoot);
	// Орбита задаёт мировую трансформацию камеры напрямую, родитель на неё не влияет.
	Camera->SetUsingAbsoluteLocation(true);
	Camera->SetUsingAbsoluteRotation(true);
	Camera->SetFieldOfView(60.f);
	Camera->bConstrainAspectRatio = false;

	OrbitCamera = CreateDefaultSubobject<UOrbitCameraComponent>(TEXT("OrbitCamera"));

	// Общий вид: ракурс «три четверти» сверху, широкий диапазон зума.
	GenplanShot.YawOffset = -35.f;
	GenplanShot.Pitch = -24.f;
	GenplanShot.FramingMargin = 1.15f;
	GenplanShot.MinShotDistance = 2000.f;
	GenplanShot.MinZoom = 0.45f;
	GenplanShot.MaxZoom = 2.2f;

	// Этаж: почти фронтально, чуть сверху, этаж занимает кадр по ширине.
	FloorShot.YawOffset = -12.f;
	FloorShot.Pitch = -16.f;
	FloorShot.FramingMargin = 1.08f;
	FloorShot.MinShotDistance = 1200.f;
	FloorShot.MinZoom = 0.35f;
	FloorShot.MaxZoom = 1.8f;

	// Квартира: близкий план с запасом вокруг объекта.
	ApartmentShot.YawOffset = -8.f;
	ApartmentShot.Pitch = -12.f;
	ApartmentShot.FramingMargin = 2.1f;
	ApartmentShot.MinShotDistance = 600.f;
	ApartmentShot.MinZoom = 0.5f;
	ApartmentShot.MaxZoom = 2.0f;
}

void AConfiguratorCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	OrbitCamera->SetControlledComponent(Camera);

	// Стартовая позиция до загрузки данных: вид издалека на центр сцены.
	FOrbitCameraRig InitialRig;
	InitialRig.Pivot = FVector(0.f, 0.f, 1500.f);
	InitialRig.Yaw = 55.f;
	InitialRig.Pitch = -25.f;
	InitialRig.Distance = 7000.f;
	OrbitCamera->SnapTo(InitialRig, FOrbitCameraLimits());

	if (UWorld* World = GetWorld())
	{
		StateSubsystem = World->GetSubsystem<UConfiguratorStateSubsystem>();
	}

	if (UConfiguratorStateSubsystem* State = StateSubsystem.Get())
	{
		State->OnViewChanged.AddUniqueDynamic(this, &AConfiguratorCameraPawn::HandleViewChanged);

		// Здание могло быть построено раньше, чем появилась пешка, — сразу занимаем актуальный ракурс.
		FOrbitCameraRig Rig;
		FOrbitCameraLimits Limits;
		if (ComputeRigForView(State->GetCurrentViewRef(), Rig, Limits))
		{
			OrbitCamera->TransitionTo(Rig, Limits);
		}
	}
	else
	{
		UE_LOG(LogConfigurator, Warning, TEXT("ConfiguratorCameraPawn: state subsystem is not available in this world"));
	}
}

void AConfiguratorCameraPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UConfiguratorStateSubsystem* State = StateSubsystem.Get())
	{
		State->OnViewChanged.RemoveDynamic(this, &AConfiguratorCameraPawn::HandleViewChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void AConfiguratorCameraPawn::AddOrbitInput(float DeltaYaw, float DeltaPitch)
{
	OrbitCamera->AddOrbitInput(DeltaYaw, DeltaPitch);
}

void AConfiguratorCameraPawn::AddZoomInput(float Steps)
{
	OrbitCamera->AddZoomInput(Steps);
}

void AConfiguratorCameraPawn::HandleViewChanged(const FConfiguratorViewState& NewView, EConfiguratorViewChangeReason Reason)
{
	FOrbitCameraRig Rig;
	FOrbitCameraLimits Limits;
	if (!ComputeRigForView(NewView, Rig, Limits))
	{
		UE_LOG(LogConfigurator, Verbose, TEXT("Camera: scene geometry for the view is not available yet"));
		return;
	}

	const float Duration = Reason == EConfiguratorViewChangeReason::Back
		? OrbitCamera->EstimateTransitionDuration(Rig) * BackTransitionSpeedScale
		: -1.f; // авто

	OrbitCamera->TransitionTo(Rig, Limits, Duration);
}

bool AConfiguratorCameraPawn::ComputeRigForView(const FConfiguratorViewState& View, FOrbitCameraRig& OutRig, FOrbitCameraLimits& OutLimits) const
{
	const UConfiguratorStateSubsystem* State = StateSubsystem.Get();
	const IConfiguratorSceneProvider* Scene = State ? State->GetSceneProvider() : nullptr;
	if (!Scene)
	{
		return false;
	}

	FBox Bounds(ForceInit);
	FVector Pivot = FVector::ZeroVector;
	const FConfiguratorShotSettings* Shot = nullptr;

	switch (View.Mode)
	{
	case EConfiguratorViewMode::Genplan:
		if (!Scene->GetBuildingBounds(Bounds))
		{
			return false;
		}
		Pivot = Bounds.GetCenter();
		Shot = &GenplanShot;
		break;

	case EConfiguratorViewMode::Floor:
		if (!Scene->GetFloorBounds(View.FloorIndex, Bounds))
		{
			return false;
		}
		Pivot = Bounds.GetCenter();
		Shot = &FloorShot;
		break;

	case EConfiguratorViewMode::Apartment:
		// Точка фокуса берётся из JSON (сцена подставляет центр объекта, если её там нет).
		if (!Scene->GetApartmentFocus(View.ApartmentId, Pivot, Bounds))
		{
			return false;
		}
		Shot = &ApartmentShot;
		break;

	default:
		return false;
	}

	OutRig.Pivot = Pivot;
	OutRig.Yaw = Scene->GetFacadeViewYaw() + Shot->YawOffset;
	OutRig.Pitch = Shot->Pitch;

	const float BaseDistance = FMath::Max(ComputeFitDistance(Bounds, Pivot, OutRig.GetRotation(), Shot->FramingMargin), Shot->MinShotDistance);
	OutRig.Distance = BaseDistance;

	OutLimits.MinDistance = BaseDistance * Shot->MinZoom;
	OutLimits.MaxDistance = BaseDistance * Shot->MaxZoom;
	OutLimits.MinPitch = Shot->MinPitch;
	OutLimits.MaxPitch = Shot->MaxPitch;
	return true;
}

float AConfiguratorCameraPawn::ComputeFitDistance(const FBox& Bounds, const FVector& Pivot, const FRotator& ViewRotation, float Margin) const
{
	// FieldOfView у UCameraComponent — горизонтальный, вертикальный выводим из соотношения сторон.
	const float TanHalfHorizontal = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Camera->FieldOfView, 10.f, 170.f) * 0.5f));
	const float TanHalfVertical = TanHalfHorizontal / ReferenceAspectRatio;

	const FRotationMatrix ViewMatrix(ViewRotation);
	const FVector Forward = ViewMatrix.GetUnitAxis(EAxis::X);
	const FVector Right = ViewMatrix.GetUnitAxis(EAxis::Y);
	const FVector Up = ViewMatrix.GetUnitAxis(EAxis::Z);

	// Точное вписывание для заданного ракурса: камера стоит на расстоянии D от точки фокуса,
	// угол бокса со смещением V от неё виден, если |V·Right| <= tan(h) * (D + V·Forward)
	// (и аналогично по вертикали). Берём минимальное D, при котором видны все 8 углов.
	// В отличие от вписывания описанной сферы, это корректно и для ракурса «три четверти»,
	// где ближний торец здания заметно ближе к камере, чем его центр.
	float RequiredDistance = 0.f;
	for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
	{
		const FVector Corner(
			(CornerIndex & 1) ? Bounds.Max.X : Bounds.Min.X,
			(CornerIndex & 2) ? Bounds.Max.Y : Bounds.Min.Y,
			(CornerIndex & 4) ? Bounds.Max.Z : Bounds.Min.Z);
		const FVector Offset = Corner - Pivot;
		const float Depth = static_cast<float>(Offset | Forward);

		RequiredDistance = FMath::Max(RequiredDistance, static_cast<float>(FMath::Abs(Offset | Right)) * Margin / TanHalfHorizontal - Depth);
		RequiredDistance = FMath::Max(RequiredDistance, static_cast<float>(FMath::Abs(Offset | Up)) * Margin / TanHalfVertical - Depth);
	}
	return RequiredDistance;
}
