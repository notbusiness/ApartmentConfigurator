#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ConfiguratorSceneProvider.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UConfiguratorSceneProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Геометрические запросы к сцене, нужные камере.
 *
 * Камера не знает, как именно построено здание (процедурно, из ассетов, из Datasmith) —
 * она спрашивает только габариты и точки фокуса через этот интерфейс. Благодаря этому
 * процедурное здание из тестового задания можно заменить реальной моделью,
 * не трогая код камеры и UI.
 */
class APARTMENTCONFIGURATOR_API IConfiguratorSceneProvider
{
	GENERATED_BODY()

public:
	/** Габариты всего здания в мировых координатах. */
	virtual bool GetBuildingBounds(FBox& OutBounds) const = 0;

	/** Габариты этажа (индекс в FBuildingData::Floors) в мировых координатах. */
	virtual bool GetFloorBounds(int32 FloorIndex, FBox& OutBounds) const = 0;

	/** Точка фокуса камеры для квартиры (из JSON либо центр объекта) и габариты объекта квартиры. */
	virtual bool GetApartmentFocus(const FString& ApartmentId, FVector& OutFocusPoint, FBox& OutBounds) const = 0;

	/** Yaw (в градусах), под которым камера смотрит на фасад здания. */
	virtual float GetFacadeViewYaw() const = 0;
};
