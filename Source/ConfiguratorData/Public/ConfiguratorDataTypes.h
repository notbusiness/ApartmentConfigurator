#pragma once

#include "CoreMinimal.h"
#include "ConfiguratorDataTypes.generated.h"

CONFIGURATORDATA_API DECLARE_LOG_CATEGORY_EXTERN(LogConfiguratorData, Log, All);

/** Статус квартиры. Unknown — значение в JSON отсутствует или не распознано. */
UENUM(BlueprintType)
enum class EApartmentStatus : uint8
{
	Available	UMETA(DisplayName = "Свободно"),
	Sold		UMETA(DisplayName = "Продано"),
	Unknown		UMETA(DisplayName = "Неизвестно")
};

/**
 * Данные одной квартиры в том виде, в каком они пришли из JSON (после валидации).
 * Чистые данные без ссылок на объекты сцены — структура безопасно создаётся
 * и копируется в фоновом потоке.
 */
USTRUCT(BlueprintType)
struct CONFIGURATORDATA_API FApartmentData
{
	GENERATED_BODY()

	/** Уникальный идентификатор (номер квартиры). */
	UPROPERTY(BlueprintReadOnly, Category = "Configurator")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Configurator")
	EApartmentStatus Status = EApartmentStatus::Unknown;

	/** Площадь в м². 0 — площадь не указана или некорректна. */
	UPROPERTY(BlueprintReadOnly, Category = "Configurator")
	float Area = 0.f;

	/** Точка фокуса камеры в локальных координатах здания (см). Имеет смысл только при bHasFocusPoint. */
	UPROPERTY(BlueprintReadOnly, Category = "Configurator")
	FVector FocusPoint = FVector::ZeroVector;

	/** false — точки фокуса в JSON нет, сцена подставит центр 3D-объекта квартиры. */
	UPROPERTY(BlueprintReadOnly, Category = "Configurator")
	bool bHasFocusPoint = false;

	bool IsSold() const { return Status == EApartmentStatus::Sold; }
	bool IsAvailable() const { return Status == EApartmentStatus::Available; }
};

/** Этаж: номер, отображаемое имя и список квартир. */
USTRUCT(BlueprintType)
struct CONFIGURATORDATA_API FFloorData
{
	GENERATED_BODY()

	/** Номер этажа из JSON (используется для сортировки и подписей). */
	UPROPERTY(BlueprintReadOnly, Category = "Configurator")
	int32 Number = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Configurator")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Configurator")
	TArray<FApartmentData> Apartments;
};

/** Корневой объект: всё здание. Индекс этажа в массиве Floors = его порядок снизу вверх. */
USTRUCT(BlueprintType)
struct CONFIGURATORDATA_API FBuildingData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Configurator")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Configurator")
	TArray<FFloorData> Floors;

	int32 GetApartmentCount() const
	{
		int32 Count = 0;
		for (const FFloorData& Floor : Floors)
		{
			Count += Floor.Apartments.Num();
		}
		return Count;
	}
};

/** Человекочитаемое имя статуса для UI. */
CONFIGURATORDATA_API FText GetApartmentStatusText(EApartmentStatus Status);
