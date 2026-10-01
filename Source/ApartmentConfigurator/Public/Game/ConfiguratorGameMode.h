#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ConfiguratorGameMode.generated.h"

class AConfiguratorBuildingActor;
class AConfiguratorSiteActor;

/**
 * Точка сборки приложения: задаёт классы пешки и контроллера, гарантирует наличие
 * здания на уровне и запускает загрузку данных.
 *
 * Логики конфигуратора здесь нет — только «склейка» независимых частей.
 */
UCLASS()
class APARTMENTCONFIGURATOR_API AConfiguratorGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AConfiguratorGameMode();

	virtual void StartPlay() override;

protected:
	/** Класс здания, которое спавнится, если на уровне его нет (можно подменить на Blueprint-наследника). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurator")
	TSubclassOf<AConfiguratorBuildingActor> BuildingClass;

	/** Где поставить здание, если его нет на уровне. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurator")
	FTransform DefaultBuildingTransform;

	/** Благоустройство (газон, дорога, деревья), спавнится вместе со зданием, если его нет на уровне. */
	UPROPERTY(EditDefaultsOnly, Category = "Configurator")
	TSubclassOf<AConfiguratorSiteActor> SiteClass;

	/**
	 * Развернуть здание (и благоустройство) фасадом к солнцу уровня (Directional Light):
	 * освещённый фасад с тенями от балконов выглядит гораздо объёмнее, чем фасад в тени.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Configurator")
	bool bAlignFacadeToSun = true;

	/** Небольшой поворот от «лоб в лоб» к солнцу, чтобы свет шёл по фасаду под углом (градусы). */
	UPROPERTY(EditDefaultsOnly, Category = "Configurator")
	float SunFacadeAngleOffset = 25.f;

	/**
	 * Высота солнца над горизонтом (градусы); 0 — оставить как на уровне.
	 * Невысокое «послеобеденное» солнце даёт длинные тени и тёплый свет: фасад читается объёмнее,
	 * чем при полуденном свете сверху.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Configurator", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float SunElevation = 32.f;

private:
	/** Спавнит актор класса Class в DefaultBuildingTransform, если на уровне ещё нет актора этого класса. */
	void SpawnIfMissing(UClass* Class);
};
