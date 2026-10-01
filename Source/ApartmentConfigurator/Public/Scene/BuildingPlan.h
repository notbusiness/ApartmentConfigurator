#pragma once

#include "CoreMinimal.h"
#include "BuildingPlan.generated.h"

struct FBuildingData;

/** Параметры раскладки здания (см). Редактируются в Details актора здания. */
USTRUCT(BlueprintType)
struct APARTMENTCONFIGURATOR_API FBuildingLayout
{
	GENERATED_BODY()

	/** Высота этажа «от пола до пола». */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "200.0"))
	float FloorHeight = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "5.0"))
	float SlabThickness = 25.f;

	/** Глубина корпуса (фасад смотрит в -Y). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "300.0"))
	float Depth = 1200.f;

	/** Номинальная ширина квартиры: ширина корпуса = макс. число квартир на этаже * это значение. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "300.0"))
	float NominalApartmentWidth = 850.f;

	/** Минимальная доля ширины относительно средней (чтобы студия не превратилась в щель). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float MinWidthShare = 0.6f;

	/** Вылет балконов за линию фасада. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0.0"))
	float BalconyDepth = 150.f;
};

/** Секция фасада одного этажа: квартира или «пустой» модуль, если на этаже нет квартир. */
struct APARTMENTCONFIGURATOR_API FFacadeModulePlan
{
	int32 FloorIndex = INDEX_NONE;
	float MinX = 0.f;
	float MaxX = 0.f;
	bool bHasBalcony = false;

	/** Индекс в FBuildingPlan::Apartments или INDEX_NONE для декоративного модуля. */
	int32 ApartmentIndex = INDEX_NONE;

	float GetWidth() const { return MaxX - MinX; }
	float GetCenterX() const { return (MinX + MaxX) * 0.5f; }
};

/** Квартира в плане здания. Все координаты — локальные координаты здания. */
struct APARTMENTCONFIGURATOR_API FApartmentPlan
{
	FString Id;
	int32 FloorIndex = INDEX_NONE;
	int32 ModuleIndex = INDEX_NONE;

	/** Кликабельный объём с подсветкой статуса: квартира целиком вместе с балконом. */
	FBox VolumeBox = FBox(ForceInit);

	/** Точка фокуса по умолчанию (перед фасадом квартиры на середине высоты этажа). */
	FVector DefaultFocus = FVector::ZeroVector;
};

/**
 * План здания: единый расчёт геометрии, которым пользуются и логика (объёмы квартир,
 * габариты для камеры), и декоративная архитектура (фасады, балконы, кровля).
 * Так раскладка считается в одном месте и не может разъехаться между слоями.
 *
 * Правила раскладки:
 *  - все этажи одной ширины (как у реального корпуса), этажи — снизу вверх по номеру;
 *  - квартиры этажа делят ширину пропорционально площади (не меньше MinWidthShare от средней),
 *    квартира без площади считается средней;
 *  - балконы — со второго этажа, у двух из трёх квартир (детерминированно, для разнообразия фасада).
 */
struct APARTMENTCONFIGURATOR_API FBuildingPlan
{
	FBuildingLayout Layout;

	float MinX = 0.f;
	float MaxX = 0.f;

	/** Отметка верха последнего этажа (низ кровли). */
	float TopZ = 0.f;

	int32 FloorCount = 0;

	TArray<FFacadeModulePlan> Modules;
	TArray<FApartmentPlan> Apartments;

	/** Габарит корпуса без кровли и благоустройства. */
	FBox Bounds = FBox(ForceInit);

	float GetFloorBaseZ(int32 FloorIndex) const { return FloorIndex * Layout.FloorHeight; }
	float GetFrontY() const { return -Layout.Depth * 0.5f; }
	float GetBackY() const { return Layout.Depth * 0.5f; }

	static FBuildingPlan Make(const FBuildingData& Data, const FBuildingLayout& Layout);
};
