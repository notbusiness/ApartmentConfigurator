#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ConfiguratorDataSettings.generated.h"

/**
 * Настройки источника данных (Project Settings -> Game -> Configurator Data, хранятся в DefaultGame.ini).
 * Путь к JSON не зашит в код: его можно поменять без пересборки.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Configurator Data"))
class CONFIGURATORDATA_API UConfiguratorDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Путь к JSON относительно папки Content проекта (допускается и абсолютный путь). */
	UPROPERTY(Config, EditAnywhere, Category = "Data")
	FString DataFilePath = TEXT("Data/Building.json");

	/**
	 * Искусственная задержка загрузки в фоновом потоке (сек). Нужна только для демонстрации
	 * того, что загрузка не блокирует интерфейс и камеру. В релизе = 0.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Data", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float SimulatedLoadDelaySeconds = 0.f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
