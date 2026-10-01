#include "Scene/ConfiguratorBuildingActor.h"

#include "ApartmentConfigurator.h"
#include "ApartmentDataSubsystem.h"
#include "Core/ConfiguratorStateSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Scene/BoxOutlineComponent.h"
#include "Scene/BuildingArchitectureComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Размер стандартного куба движка в сантиметрах. */
	constexpr double BuildingCubeMeshSize = 100.0;

	/** Высота кровли с парапетом и надстройкой над последним этажом (для кадрирования общего вида). */
	constexpr float RoofStructuresHeight = 380.f;
}

AConfiguratorBuildingActor::AConfiguratorBuildingActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Architecture = CreateDefaultSubobject<UBuildingArchitectureComponent>(TEXT("Architecture"));
	Architecture->SetupAttachment(SceneRoot);

	SelectionOutline = CreateDefaultSubobject<UBoxOutlineComponent>(TEXT("SelectionOutline"));
	SelectionOutline->SetupAttachment(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		CubeMesh = CubeFinder.Object;
	}

	// Мастер-материалы проекта создаются скриптом Tools/CreateMaterials.py.
	// Если их нет, здание всё равно строится на базовом материале движка (без стекла и подсветки).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SurfaceFinder(TEXT("/Game/Configurator/Materials/M_ConfiguratorSurface.M_ConfiguratorSurface"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OverlayFinder(TEXT("/Game/Configurator/Materials/M_ConfiguratorOverlay.M_ConfiguratorOverlay"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FallbackFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	SurfaceMaterial = SurfaceFinder.Succeeded() ? SurfaceFinder.Object : FallbackFinder.Object;
	OverlayMaterial = OverlayFinder.Succeeded() ? OverlayFinder.Object : FallbackFinder.Object;
}

void AConfiguratorBuildingActor::BeginPlay()
{
	Super::BeginPlay();

	Architecture->CubeMesh = CubeMesh;
	Architecture->SurfaceMaterial = SurfaceMaterial;
	SelectionOutline->Setup(CubeMesh, OverlayMaterial);

	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	StateSubsystem = World ? World->GetSubsystem<UConfiguratorStateSubsystem>() : nullptr;
	DataSubsystem = GameInstance ? GameInstance->GetSubsystem<UApartmentDataSubsystem>() : nullptr;

	if (UConfiguratorStateSubsystem* State = StateSubsystem.Get())
	{
		State->OnViewChanged.AddUniqueDynamic(this, &AConfiguratorBuildingActor::HandleViewChanged);
		State->OnFilterChanged.AddUniqueDynamic(this, &AConfiguratorBuildingActor::HandleFilterChanged);
		State->RegisterSceneProvider(this);
		bHideSold = State->IsHideSold();
		CurrentView = State->GetCurrentViewRef();
	}

	if (UApartmentDataSubsystem* Data = DataSubsystem.Get())
	{
		Data->OnDataLoaded.AddUniqueDynamic(this, &AConfiguratorBuildingActor::HandleDataLoaded);

		// Данные могли загрузиться раньше появления актора (например, после смены уровня).
		if (Data->HasData())
		{
			HandleDataLoaded();
		}
	}

	if (!CubeMesh || !SurfaceMaterial || !OverlayMaterial)
	{
		UE_LOG(LogConfigurator, Error, TEXT("ConfiguratorBuildingActor: mesh or materials are missing, building will not be visible"));
	}
}

void AConfiguratorBuildingActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UConfiguratorStateSubsystem* State = StateSubsystem.Get())
	{
		State->OnViewChanged.RemoveDynamic(this, &AConfiguratorBuildingActor::HandleViewChanged);
		State->OnFilterChanged.RemoveDynamic(this, &AConfiguratorBuildingActor::HandleFilterChanged);
		State->UnregisterSceneProvider(this);
	}
	if (UApartmentDataSubsystem* Data = DataSubsystem.Get())
	{
		Data->OnDataLoaded.RemoveDynamic(this, &AConfiguratorBuildingActor::HandleDataLoaded);
	}

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------- Реакция на события

void AConfiguratorBuildingActor::HandleDataLoaded()
{
	const UApartmentDataSubsystem* Data = DataSubsystem.Get();
	if (!Data)
	{
		return;
	}

	RebuildFromData(Data->GetBuildingData());

	// Сообщаем о готовой геометрии: подсистема состояния сбросит вид, камера перелетит к общему плану.
	if (UConfiguratorStateSubsystem* State = StateSubsystem.Get())
	{
		State->NotifySceneRebuilt();
	}
}

void AConfiguratorBuildingActor::HandleViewChanged(const FConfiguratorViewState& NewView, EConfiguratorViewChangeReason Reason)
{
	CurrentView = NewView;
	RefreshVisuals();
	UpdateSelectionOutline();
}

void AConfiguratorBuildingActor::HandleFilterChanged(bool bInHideSold)
{
	bHideSold = bInHideSold;
	RefreshVisuals();
}

// ---------------------------------------------------------------- Построение

void AConfiguratorBuildingActor::ClearApartments()
{
	for (UApartmentMeshComponent* Apartment : ApartmentComponents)
	{
		if (Apartment)
		{
			Apartment->DestroyComponent();
		}
	}

	ApartmentComponents.Reset();
	ApartmentById.Reset();
	FocusPoints.Reset();
	ApartmentLocalBoxes.Reset();
	FloorBounds.Reset();
	BuildingBounds = FBox(ForceInit);
	SelectionOutline->HideOutline();
}

void AConfiguratorBuildingActor::RebuildFromData(const FBuildingData& Data)
{
	ClearApartments();

	if (!CubeMesh)
	{
		return;
	}

	// Единый план: из него строятся и архитектура, и объёмы квартир, и габариты для камеры.
	const FBuildingPlan Plan = FBuildingPlan::Make(Data, Layout);
	Architecture->Build(Plan);

	const FTransform& ActorTransform = GetActorTransform();

	for (const FApartmentPlan& ApartmentPlan : Plan.Apartments)
	{
		const FFloorData& Floor = Data.Floors[ApartmentPlan.FloorIndex];
		const FApartmentData* ApartmentData = Floor.Apartments.FindByPredicate(
			[&ApartmentPlan](const FApartmentData& Candidate) { return Candidate.Id == ApartmentPlan.Id; });
		if (!ApartmentData)
		{
			continue;
		}

		UApartmentMeshComponent* Apartment = NewObject<UApartmentMeshComponent>(this);
		Apartment->SetMobility(EComponentMobility::Movable);
		Apartment->SetStaticMesh(CubeMesh);
		Apartment->SetupAttachment(SceneRoot);
		Apartment->SetRelativeLocation(ApartmentPlan.VolumeBox.GetCenter());
		Apartment->SetRelativeScale3D(ApartmentPlan.VolumeBox.GetSize() / BuildingCubeMeshSize);
		Apartment->InitializeApartment(*ApartmentData, ApartmentPlan.FloorIndex, OverlayMaterial, Palette.OverlayEmissive);
		Apartment->RegisterComponent();
		AddInstanceComponent(Apartment);

		ApartmentComponents.Add(Apartment);
		ApartmentById.Add(ApartmentData->Id, Apartment);
		ApartmentLocalBoxes.Add(ApartmentData->Id, ApartmentPlan.VolumeBox);

		// Точка фокуса из JSON задана в локальных координатах здания; нет её — берём точку из плана.
		const FVector LocalFocus = ApartmentData->bHasFocusPoint ? ApartmentData->FocusPoint : ApartmentPlan.DefaultFocus;
		FocusPoints.Add(ApartmentData->Id, ActorTransform.TransformPosition(LocalFocus));
	}

	// Габариты этажей (с балконами) и всего здания (с кровлей) — для кадрирования камерой.
	FloorBounds.SetNum(Plan.FloorCount);
	for (int32 FloorIndex = 0; FloorIndex < Plan.FloorCount; ++FloorIndex)
	{
		const float BaseZ = Plan.GetFloorBaseZ(FloorIndex);
		const FBox LocalFloor(
			FVector(Plan.MinX, Plan.GetFrontY() - Layout.BalconyDepth, BaseZ),
			FVector(Plan.MaxX, Plan.GetBackY(), BaseZ + Layout.FloorHeight));
		FloorBounds[FloorIndex] = LocalFloor.TransformBy(ActorTransform);
	}

	FBox LocalBuilding = Plan.Bounds;
	LocalBuilding.Max.Z += RoofStructuresHeight;
	BuildingBounds = LocalBuilding.TransformBy(ActorTransform);

	RefreshVisuals();

	UE_LOG(LogConfigurator, Log, TEXT("Building rebuilt: %d floors, %d apartments"), Plan.FloorCount, ApartmentComponents.Num());
}

// ---------------------------------------------------------------- Визуальное состояние

EApartmentVisualState AConfiguratorBuildingActor::ResolveVisualState(const UApartmentMeshComponent& Apartment) const
{
	if (CurrentView.Mode == EConfiguratorViewMode::Apartment && CurrentView.ApartmentId == Apartment.GetApartmentId())
	{
		return EApartmentVisualState::Selected;
	}

	switch (Apartment.GetStatus())
	{
	case EApartmentStatus::Available:	return EApartmentVisualState::Available;
	case EApartmentStatus::Sold:		return bHideSold ? EApartmentVisualState::SoldHidden : EApartmentVisualState::Sold;
	default:							return EApartmentVisualState::Unknown;
	}
}

FLinearColor AConfiguratorBuildingActor::GetColorForState(EApartmentVisualState State) const
{
	switch (State)
	{
	case EApartmentVisualState::Available:	return Palette.Available;
	case EApartmentVisualState::Sold:		return Palette.Sold;
	case EApartmentVisualState::SoldHidden:	return Palette.SoldHidden;
	case EApartmentVisualState::Selected:	return Palette.Selected;
	default:								return Palette.Unknown;
	}
}

float AConfiguratorBuildingActor::GetOpacityFor(const UApartmentMeshComponent& Apartment, EApartmentVisualState State) const
{
	if (State == EApartmentVisualState::Selected)
	{
		return Palette.SelectedOpacity;
	}
	if (State == EApartmentVisualState::SoldHidden)
	{
		return Palette.SoldHiddenOpacity;
	}
	if (CurrentView.Mode == EConfiguratorViewMode::Genplan)
	{
		return Palette.GenplanOpacity;
	}
	return Apartment.GetFloorIndex() == CurrentView.FloorIndex ? Palette.ActiveFloorOpacity : Palette.InactiveFloorOpacity;
}

void AConfiguratorBuildingActor::RefreshVisuals()
{
	// Вызывается только по событиям (смена вида/фильтра), а ApplyOverlay пропускает
	// неизменившиеся квартиры — даже для сотен квартир это доли миллисекунды.
	for (UApartmentMeshComponent* Apartment : ApartmentComponents)
	{
		if (Apartment)
		{
			const EApartmentVisualState State = ResolveVisualState(*Apartment);
			Apartment->ApplyOverlay(GetColorForState(State), GetOpacityFor(*Apartment, State), State != EApartmentVisualState::SoldHidden);
		}
	}
}

void AConfiguratorBuildingActor::UpdateSelectionOutline()
{
	// Контур прикреплён к корню здания без смещения, поэтому принимает бокс в локальных координатах здания.
	if (CurrentView.Mode == EConfiguratorViewMode::Apartment)
	{
		if (const FBox* LocalBox = ApartmentLocalBoxes.Find(CurrentView.ApartmentId))
		{
			SelectionOutline->ShowAroundBox(*LocalBox);
			return;
		}
	}
	SelectionOutline->HideOutline();
}

// ---------------------------------------------------------------- IConfiguratorSceneProvider

bool AConfiguratorBuildingActor::GetBuildingBounds(FBox& OutBounds) const
{
	OutBounds = BuildingBounds;
	return BuildingBounds.IsValid != 0;
}

bool AConfiguratorBuildingActor::GetFloorBounds(int32 FloorIndex, FBox& OutBounds) const
{
	if (!FloorBounds.IsValidIndex(FloorIndex) || !FloorBounds[FloorIndex].IsValid)
	{
		return false;
	}
	OutBounds = FloorBounds[FloorIndex];
	return true;
}

bool AConfiguratorBuildingActor::GetApartmentFocus(const FString& ApartmentId, FVector& OutFocusPoint, FBox& OutBounds) const
{
	const FVector* Focus = FocusPoints.Find(ApartmentId);
	const FBox* LocalBox = ApartmentLocalBoxes.Find(ApartmentId);
	if (!Focus || !LocalBox)
	{
		return false;
	}

	OutFocusPoint = *Focus;
	OutBounds = LocalBox->TransformBy(GetActorTransform());
	return true;
}

float AConfiguratorBuildingActor::GetFacadeViewYaw() const
{
	// Фасад смотрит в -Y локальных координат, значит камера перед ним смотрит в +Y (yaw = 90).
	return static_cast<float>(GetActorRotation().Yaw) + 90.f;
}
