#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "ConfiguratorDataTypes.h"
#include "ApartmentMeshComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;

/** Визуальное состояние квартиры (цвет подсветки). */
UENUM(BlueprintType)
enum class EApartmentVisualState : uint8
{
	Available,
	Sold,
	/** Продана и включён фильтр «Скрыть проданные»: затемнена и не реагирует на клик. */
	SoldHidden,
	Unknown,
	Selected
};

/**
 * Объём квартиры: полупрозрачная подсветка статуса поверх архитектуры и одновременно
 * цель для клика (трассировка по каналу Visibility). Хранит ID квартиры — по нему клик
 * в сцене связывается с данными.
 *
 * Архитектура (окна, балконы) рисуется отдельно и коллизии не имеет, поэтому клик по
 * любой части квартиры на фасаде попадает сюда. Компонент, а не актор на квартиру:
 * квартире не нужны ни тик, ни логика — только меш, материал и коллизия.
 */
UCLASS(ClassGroup = (Configurator))
class APARTMENTCONFIGURATOR_API UApartmentMeshComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UApartmentMeshComponent();

	/** Привязка к данным и создание собственного экземпляра полупрозрачного материала. */
	void InitializeApartment(const FApartmentData& Data, int32 InFloorIndex, UMaterialInterface* InOverlayMaterial, float Emissive);

	/**
	 * Цвет и прозрачность подсветки, доступность для клика. Повторный вызов с теми же
	 * значениями ничего не делает — обновление всех квартир по событию стоит копейки.
	 * При нулевой прозрачности объём не рисуется, но остаётся кликабельным.
	 */
	void ApplyOverlay(const FLinearColor& Color, float Opacity, bool bClickable);

	const FString& GetApartmentId() const { return ApartmentId; }
	int32 GetFloorIndex() const { return FloorIndex; }
	EApartmentStatus GetStatus() const { return Status; }

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Apartment")
	FString ApartmentId;

	UPROPERTY(VisibleInstanceOnly, Category = "Apartment")
	int32 FloorIndex = INDEX_NONE;

	UPROPERTY(VisibleInstanceOnly, Category = "Apartment")
	EApartmentStatus Status = EApartmentStatus::Unknown;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	/** Последние применённые значения (для пропуска повторных вызовов). */
	FLinearColor AppliedColor = FLinearColor::Transparent;
	float AppliedOpacity = -1.f;
	bool bAppliedClickable = true;
};
