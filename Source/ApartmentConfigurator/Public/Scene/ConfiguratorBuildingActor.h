#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/ConfiguratorSceneProvider.h"
#include "Core/ConfiguratorViewTypes.h"
#include "Scene/ApartmentMeshComponent.h"
#include "Scene/BuildingPlan.h"
#include "ConfiguratorBuildingActor.generated.h"

class UApartmentDataSubsystem;
class UBoxOutlineComponent;
class UBuildingArchitectureComponent;
class UConfiguratorStateSubsystem;
class UMaterialInterface;
class UStaticMesh;
struct FBuildingData;

/**
 * Подсветка квартир поверх архитектуры: цвета статусов и прозрачность в разных режимах.
 * Вынесено в свойства — дизайнер настраивает без перекомпиляции.
 */
USTRUCT(BlueprintType)
struct APARTMENTCONFIGURATOR_API FApartmentPalette
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Available = FLinearColor(0.05f, 0.85f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Sold = FLinearColor(0.95f, 0.12f, 0.08f);

	/** Проданные при включённом фильтре — затемнены. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor SoldHidden = FLinearColor(0.004f, 0.004f, 0.006f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Unknown = FLinearColor(0.45f, 0.45f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor Selected = FLinearColor(1.f, 0.62f, 0.05f);

	/** Общий вид: лёгкая подсветка статусов, архитектура остаётся главной. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Opacity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GenplanOpacity = 0.06f;

	/** Квартиры выбранного этажа в режимах Floor / Apartment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Opacity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ActiveFloorOpacity = 0.3f;

	/** Остальные этажи в режимах Floor / Apartment (0 — без подсветки, этаж «в фокусе»). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Opacity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InactiveFloorOpacity = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Opacity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SelectedOpacity = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Opacity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SoldHiddenOpacity = 0.8f;

	/** Яркость подсветки: unlit-материал светится сам, поэтому < 1 — тонировка, а не «неон». */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Opacity", meta = (ClampMin = "0.0"))
	float OverlayEmissive = 0.55f;
};

/**
 * Здание по данным из JSON.
 *
 * Роли актора:
 *  - по FBuildingPlan строит архитектуру (UBuildingArchitectureComponent) и объёмы квартир
 *    (UApartmentMeshComponent — подсветка статуса и цель клика);
 *  - отражает навигационное состояние визуально: выбранная квартира, активный этаж, фильтр;
 *  - отвечает на геометрические запросы камеры через IConfiguratorSceneProvider.
 *
 * Все обновления событийные: тик у актора выключен.
 */
UCLASS()
class APARTMENTCONFIGURATOR_API AConfiguratorBuildingActor : public AActor, public IConfiguratorSceneProvider
{
	GENERATED_BODY()

public:
	AConfiguratorBuildingActor();

	// IConfiguratorSceneProvider
	virtual bool GetBuildingBounds(FBox& OutBounds) const override;
	virtual bool GetFloorBounds(int32 FloorIndex, FBox& OutBounds) const override;
	virtual bool GetApartmentFocus(const FString& ApartmentId, FVector& OutFocusPoint, FBox& OutBounds) const override;
	virtual float GetFacadeViewYaw() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleDataLoaded();

	UFUNCTION()
	void HandleViewChanged(const FConfiguratorViewState& NewView, EConfiguratorViewChangeReason Reason);

	UFUNCTION()
	void HandleFilterChanged(bool bHideSold);

	/** Полная перестройка здания по данным. */
	void RebuildFromData(const FBuildingData& Data);
	void ClearApartments();

	/** Пересчитывает подсветку и кликабельность всех квартир по текущему состоянию. */
	void RefreshVisuals();
	void UpdateSelectionOutline();

	EApartmentVisualState ResolveVisualState(const UApartmentMeshComponent& Apartment) const;
	FLinearColor GetColorForState(EApartmentVisualState State) const;
	float GetOpacityFor(const UApartmentMeshComponent& Apartment, EApartmentVisualState State) const;

	// ---------------------------------------------------------------- Компоненты

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBuildingArchitectureComponent> Architecture;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxOutlineComponent> SelectionOutline;

	// ---------------------------------------------------------------- Ассеты и внешний вид

	/** Меш-куб 100x100x100 с пивотом в центре. По умолчанию — /Engine/BasicShapes/Cube. */
	UPROPERTY(EditAnywhere, Category = "Appearance")
	TObjectPtr<UStaticMesh> CubeMesh;

	/** Непрозрачный материал архитектуры (Color / Roughness / Metallic). */
	UPROPERTY(EditAnywhere, Category = "Appearance")
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

	/** Полупрозрачный unlit-материал подсветки квартир и контура (Color / Emissive / Opacity). */
	UPROPERTY(EditAnywhere, Category = "Appearance")
	TObjectPtr<UMaterialInterface> OverlayMaterial;

	UPROPERTY(EditAnywhere, Category = "Appearance")
	FApartmentPalette Palette;

	UPROPERTY(EditAnywhere, Category = "Layout")
	FBuildingLayout Layout;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UApartmentMeshComponent>> ApartmentComponents;

	/** Быстрый доступ по ID для камеры и контура выделения. */
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UApartmentMeshComponent>> ApartmentById;

	/** Точки фокуса в мировых координатах (из JSON или по плану здания). */
	TMap<FString, FVector> FocusPoints;

	/** Объёмы квартир в локальных координатах здания — для контура и кадрирования. */
	TMap<FString, FBox> ApartmentLocalBoxes;

	/** Габариты этажей и здания в мировых координатах — считаются один раз при построении. */
	TArray<FBox> FloorBounds;
	FBox BuildingBounds = FBox(ForceInit);

	/** Кэш текущего состояния навигации и фильтра. */
	FConfiguratorViewState CurrentView;
	bool bHideSold = false;

	TWeakObjectPtr<UConfiguratorStateSubsystem> StateSubsystem;
	TWeakObjectPtr<UApartmentDataSubsystem> DataSubsystem;
};
