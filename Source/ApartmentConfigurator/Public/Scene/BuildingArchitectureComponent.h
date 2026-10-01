#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Scene/SurfaceLook.h"
#include "BuildingArchitectureComponent.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
struct FBuildingPlan;
struct FFacadeModulePlan;

/** Части архитектуры: у каждой свой материал и свой инстанс-компонент. */
enum class EArchitecturePart : uint8
{
	Wall,
	WallAlt,
	Accent,
	Glass,
	Frame,
	Concrete,
	Metal,
	Trim,
	Plinth,
	Roof,
	Count
};

/**
 * Декоративная архитектура здания по FBuildingPlan: перекрытия, фасады с окнами и рамами,
 * пилястры между квартирами, балконы с ограждениями, кондиционеры, торцы, кровля с парапетом,
 * цоколь, вход. Фактуры (кирпич, штукатурка, бетон, стекло) — процедурные, см. FSurfaceLook.
 *
 * Производительность: вся геометрия — инстансы одного куба, по одному
 * UInstancedStaticMeshComponent на материал. Несколько тысяч деталей здания рисуются
 * за ~8 вызовов отрисовки. Коллизии у архитектуры нет: клики попадают в объёмы квартир.
 *
 * Компонент ничего не знает о статусах, выборе и камере — только строит «коробку» дома.
 */
UCLASS(ClassGroup = (Configurator))
class APARTMENTCONFIGURATOR_API UBuildingArchitectureComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UBuildingArchitectureComponent();

	/** Перестроить архитектуру по плану (предыдущие инстансы удаляются). */
	void Build(const FBuildingPlan& Plan);

	void Clear();

	/** Меш-куб 100x100x100 с пивотом в центре. */
	UPROPERTY(EditAnywhere, Category = "Architecture")
	TObjectPtr<UStaticMesh> CubeMesh;

	/** Мастер-материал с параметрами Color / Roughness / Metallic. */
	UPROPERTY(EditAnywhere, Category = "Architecture")
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

	UPROPERTY(EditAnywhere, Category = "Look")
	FSurfaceLook WallLook;

	/** Второй тон фасада: часть квартирных секций выделена цветом, фасад не выглядит штампованным. */
	UPROPERTY(EditAnywhere, Category = "Look")
	FSurfaceLook AltWallLook;

	/** Пилястры между квартирами и парапет. */
	UPROPERTY(EditAnywhere, Category = "Look")
	FSurfaceLook AccentLook;

	UPROPERTY(EditAnywhere, Category = "Look")
	FSurfaceLook GlassLook;

	UPROPERTY(EditAnywhere, Category = "Look")
	FSurfaceLook FrameLook;

	/** Перекрытия, плиты балконов, отливы. */
	UPROPERTY(EditAnywhere, Category = "Look")
	FSurfaceLook ConcreteLook;

	/** Ограждения балконов, кровельное оборудование. */
	UPROPERTY(EditAnywhere, Category = "Look")
	FSurfaceLook MetalLook;

	/** Светлая отделка: крышка парапета, наружные блоки кондиционеров. */
	UPROPERTY(EditAnywhere, Category = "Look")
	FSurfaceLook TrimLook;

	UPROPERTY(EditAnywhere, Category = "Look")
	FSurfaceLook PlinthLook;

	UPROPERTY(EditAnywhere, Category = "Look")
	FSurfaceLook RoofLook;

private:
	/** Прямоугольный проём в стене модуля (координаты X и Z, локальные). */
	struct FOpening
	{
		float MinX;
		float MaxX;
		float MinZ;
		float MaxZ;
	};

	void AddBox(EArchitecturePart Part, const FVector& Min, const FVector& Max);

	void BuildFloorShell(const FBuildingPlan& Plan, int32 FloorIndex);
	void BuildFacadeModule(const FBuildingPlan& Plan, const FFacadeModulePlan& Module, bool bFront);
	void BuildWallWithOpenings(EArchitecturePart WallPart, float MinX, float MaxX, float MinZ, float MaxZ, float OuterY, float InwardSign, TArray<FOpening> Openings);
	void BuildWindow(const FOpening& Opening, float OuterY, float InwardSign);
	void BuildAirConditioner(const FOpening& Window, float OuterY, float InwardSign);
	void BuildBalcony(float MinX, float MaxX, float FloorZ, float FacadeY, float Depth);
	void BuildRoof(const FBuildingPlan& Plan);
	void BuildBaseAndEntrance(const FBuildingPlan& Plan);

	UInstancedStaticMeshComponent* GetOrCreatePartComponent(EArchitecturePart Part);
	const FSurfaceLook& GetLook(EArchitecturePart Part) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> PartComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PartMaterials;

	/** Трансформы, накопленные за Build(): добавляются в компоненты одним пакетом на материал. */
	TArray<TArray<FTransform>> PendingInstances;
};
