#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ConfiguratorDataTypes.h"
#include "ApartmentDataSubsystem.generated.h"

struct FConfiguratorParseResult;

UENUM(BlueprintType)
enum class EConfiguratorLoadState : uint8
{
	NotLoaded,
	Loading,
	Loaded,
	Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnConfiguratorDataLoaded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConfiguratorDataLoadFailed, const FString&, ErrorMessage);

/**
 * Хранилище данных здания и точка входа для их загрузки.
 *
 * Почему UGameInstanceSubsystem:
 *  - живёт всё время работы приложения и переживает смену уровней — данные грузятся один раз;
 *  - создаётся движком автоматически, не требует ручного синглтона или актора на сцене;
 *  - доступен из любого места (C++ и Blueprint) через GameInstance.
 *
 * Загрузка асинхронная: чтение файла и парсинг выполняются в пуле потоков,
 * а результат применяется на игровом потоке. UI в это время продолжает работать.
 * При ошибке уже загруженные данные не теряются (старые данные остаются активными).
 */
UCLASS()
class CONFIGURATORDATA_API UApartmentDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Загрузить файл, указанный в настройках проекта (UConfiguratorDataSettings). */
	UFUNCTION(BlueprintCallable, Category = "Configurator|Data")
	void RequestLoad();

	/** Загрузить произвольный файл. Относительный путь считается от папки Content. */
	UFUNCTION(BlueprintCallable, Category = "Configurator|Data")
	void LoadFromFile(const FString& FilePath);

	UFUNCTION(BlueprintPure, Category = "Configurator|Data")
	EConfiguratorLoadState GetLoadState() const { return LoadState; }

	UFUNCTION(BlueprintPure, Category = "Configurator|Data")
	bool HasData() const { return bHasData; }

	/** Текущие данные здания (для C++, без копирования). */
	const FBuildingData& GetBuildingData() const { return BuildingData; }

	/** Blueprint-версия GetBuildingData (возвращает копию). */
	UFUNCTION(BlueprintPure, Category = "Configurator|Data", meta = (DisplayName = "Get Building Data"))
	FBuildingData GetBuildingDataCopy() const { return BuildingData; }

	/** Предупреждения последнего успешного разбора (для отображения в UI/логе). */
	UFUNCTION(BlueprintPure, Category = "Configurator|Data")
	TArray<FString> GetLastWarnings() const { return LastWarnings; }

	UFUNCTION(BlueprintPure, Category = "Configurator|Data")
	FString GetLastError() const { return LastError; }

	/** Поиск квартиры по ID за O(1). Возвращает nullptr, если такой нет. */
	const FApartmentData* FindApartment(const FString& ApartmentId, int32* OutFloorIndex = nullptr) const;

	/** Blueprint-версия FindApartment (возвращает копию). */
	UFUNCTION(BlueprintCallable, Category = "Configurator|Data", meta = (DisplayName = "Find Apartment"))
	bool FindApartmentCopy(const FString& ApartmentId, FApartmentData& OutApartment, int32& OutFloorIndex) const;

	/** Данные успешно загружены/перезагружены. Актуальные данные — GetBuildingData(). */
	UPROPERTY(BlueprintAssignable, Category = "Configurator|Data")
	FOnConfiguratorDataLoaded OnDataLoaded;

	/** Загрузка или парсинг не удались. Предыдущие данные (если были) остаются в силе. */
	UPROPERTY(BlueprintAssignable, Category = "Configurator|Data")
	FOnConfiguratorDataLoadFailed OnLoadFailed;

private:
	/** Вызывается на игровом потоке, когда фоновая задача завершилась. */
	void ApplyParseResult(uint32 RequestId, FConfiguratorParseResult&& Result);

	void RebuildApartmentIndex();

	UPROPERTY(Transient)
	FBuildingData BuildingData;

	TArray<FString> LastWarnings;
	FString LastError;

	/** ID квартиры -> (индекс этажа, индекс квартиры на этаже). */
	TMap<FString, FIntPoint> ApartmentIndex;

	EConfiguratorLoadState LoadState = EConfiguratorLoadState::NotLoaded;
	bool bHasData = false;

	/**
	 * Номер последнего запроса. Если пользователь запросил перезагрузку, пока предыдущая
	 * ещё не завершилась, устаревший результат просто отбрасывается.
	 */
	uint32 LatestRequestId = 0;
};
