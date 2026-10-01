#pragma once

#include "CoreMinimal.h"
#include "ConfiguratorViewTypes.generated.h"

/** Режим просмотра (он же режим камеры). */
UENUM(BlueprintType)
enum class EConfiguratorViewMode : uint8
{
	/** Общий вид: всё здание, свободное вращение. */
	Genplan,
	/** Вид выбранного этажа. */
	Floor,
	/** Приближение к выбранной квартире. */
	Apartment
};

/** Почему изменился вид — подписчики могут реагировать по-разному (например, длительность перелёта). */
UENUM(BlueprintType)
enum class EConfiguratorViewChangeReason : uint8
{
	/** Обычный переход по действию пользователя (запись попадает в историю). */
	Navigate,
	/** Отмена последнего действия кнопкой «Назад». */
	Back,
	/** Сброс после (пере)загрузки данных: история очищена. */
	Reset
};

/**
 * Снимок навигационного состояния. Хранится в стеке истории, поэтому
 * содержит только идентификаторы (индекс этажа, ID квартиры), а не указатели на объекты сцены:
 * после перезагрузки данных устаревший снимок не превратится в висячую ссылку.
 */
USTRUCT(BlueprintType)
struct APARTMENTCONFIGURATOR_API FConfiguratorViewState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Configurator")
	EConfiguratorViewMode Mode = EConfiguratorViewMode::Genplan;

	/** Индекс этажа в FBuildingData::Floors (INDEX_NONE в общем виде). */
	UPROPERTY(BlueprintReadOnly, Category = "Configurator")
	int32 FloorIndex = INDEX_NONE;

	/** ID квартиры (пусто, если квартира не выбрана). */
	UPROPERTY(BlueprintReadOnly, Category = "Configurator")
	FString ApartmentId;

	static FConfiguratorViewState MakeGenplan()
	{
		return FConfiguratorViewState();
	}

	static FConfiguratorViewState MakeFloor(int32 InFloorIndex)
	{
		FConfiguratorViewState State;
		State.Mode = EConfiguratorViewMode::Floor;
		State.FloorIndex = InFloorIndex;
		return State;
	}

	static FConfiguratorViewState MakeApartment(int32 InFloorIndex, const FString& InApartmentId)
	{
		FConfiguratorViewState State;
		State.Mode = EConfiguratorViewMode::Apartment;
		State.FloorIndex = InFloorIndex;
		State.ApartmentId = InApartmentId;
		return State;
	}

	bool operator==(const FConfiguratorViewState& Other) const
	{
		return Mode == Other.Mode && FloorIndex == Other.FloorIndex && ApartmentId.Equals(Other.ApartmentId, ESearchCase::CaseSensitive);
	}

	bool operator!=(const FConfiguratorViewState& Other) const
	{
		return !(*this == Other);
	}
};
