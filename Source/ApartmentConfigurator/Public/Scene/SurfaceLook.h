#pragma once

#include "CoreMinimal.h"
#include "SurfaceLook.generated.h"

class UMaterialInstanceDynamic;

/**
 * Процедурная фактура поверхности. Значения совпадают с параметром Pattern мастер-материала
 * M_ConfiguratorSurface (см. Tools/CreateMaterials.py): рисунок и рельеф считаются в шейдере
 * по положению точки, без текстур.
 */
UENUM(BlueprintType)
enum class ESurfacePattern : uint8
{
	Plain = 0			UMETA(DisplayName = "Однотонная"),
	Plaster = 1			UMETA(DisplayName = "Штукатурка"),
	Brick = 2			UMETA(DisplayName = "Кирпич"),
	Paving = 3			UMETA(DisplayName = "Тротуарная плитка"),
	Grass = 4			UMETA(DisplayName = "Газон"),
	Asphalt = 5			UMETA(DisplayName = "Асфальт / кровля"),
	Glass = 6			UMETA(DisplayName = "Оконное стекло"),
	Foliage = 7			UMETA(DisplayName = "Листва"),
	Concrete = 8		UMETA(DisplayName = "Бетон"),
	DistantBuilding = 9	UMETA(DisplayName = "Дальняя застройка")
};

/** Внешний вид поверхности: набор параметров мастер-материала M_ConfiguratorSurface. */
USTRUCT(BlueprintType)
struct APARTMENTCONFIGURATOR_API FSurfaceLook
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FLinearColor Color = FLinearColor(0.7f, 0.7f, 0.7f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Roughness = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Metallic = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	ESurfacePattern Pattern = ESurfacePattern::Plain;

	FSurfaceLook() = default;

	FSurfaceLook(const FLinearColor& InColor, float InRoughness, float InMetallic = 0.f, ESurfacePattern InPattern = ESurfacePattern::Plain)
		: Color(InColor), Roughness(InRoughness), Metallic(InMetallic), Pattern(InPattern)
	{
	}

	/**
	 * Записывает параметры в экземпляр материала. ActorTransform — трансформация актора-владельца:
	 * рисунок раскладывается в его пространстве, поэтому кирпич остаётся выровнен по стенам
	 * при любом повороте здания.
	 */
	void ApplyTo(UMaterialInstanceDynamic& Material, const FTransform& ActorTransform) const;
};
