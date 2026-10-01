#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/ConfiguratorViewTypes.h"
#include "ConfiguratorStateSubsystem.generated.h"

class IConfiguratorSceneProvider;
class UApartmentDataSubsystem;
struct FApartmentData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnConfiguratorViewChanged, const FConfiguratorViewState&, NewView, EConfiguratorViewChangeReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConfiguratorFilterChanged, bool, bHideSold);

/**
 * Навигационное состояние конфигуратора — единственный источник правды о том,
 * что сейчас выбрано: режим, этаж, квартира, фильтр, история для кнопки «Назад».
 *
 * Схема взаимодействия (однонаправленный поток данных):
 *   UI / клик по сцене  --команды-->  UConfiguratorStateSubsystem  --события-->  камера, сцена, UI
 *
 * Ни камера, ни UI, ни здание не знают друг о друге: они только отправляют команды
 * и подписываются на события. Поэтому, например, выбор квартиры кнопкой в панели
 * и кликом по 3D-объекту проходит один и тот же путь.
 *
 * Почему UWorldSubsystem: состояние навигации относится к конкретному миру (уровню)
 * и должно сбрасываться вместе с ним, а данные здания (UApartmentDataSubsystem) живут дольше —
 * на уровне GameInstance.
 */
UCLASS()
class APARTMENTCONFIGURATOR_API UConfiguratorStateSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// ---------------------------------------------------------------- Навигация

	/** Перейти к общему виду. */
	UFUNCTION(BlueprintCallable, Category = "Configurator|Navigation")
	void ShowGenplan();

	/** Перейти к виду этажа (индекс в FBuildingData::Floors). */
	UFUNCTION(BlueprintCallable, Category = "Configurator|Navigation")
	void ShowFloor(int32 FloorIndex);

	/** Сфокусироваться на квартире. Возвращает false, если квартира не найдена или скрыта фильтром. */
	UFUNCTION(BlueprintCallable, Category = "Configurator|Navigation")
	bool ShowApartment(const FString& ApartmentId);

	/** Отменить последнее навигационное действие. Возвращает false, если история пуста. */
	UFUNCTION(BlueprintCallable, Category = "Configurator|Navigation")
	bool GoBack();

	UFUNCTION(BlueprintPure, Category = "Configurator|Navigation")
	bool CanGoBack() const { return History.Num() > 0; }

	UFUNCTION(BlueprintPure, Category = "Configurator|Navigation")
	FConfiguratorViewState GetCurrentView() const { return CurrentView; }

	/** C++-доступ к текущему виду без копирования. */
	const FConfiguratorViewState& GetCurrentViewRef() const { return CurrentView; }

	// ---------------------------------------------------------------- Фильтр

	/** Фильтр «Скрыть проданные». Не записывается в историю: это настройка отображения, а не переход. */
	UFUNCTION(BlueprintCallable, Category = "Configurator|Filter")
	void SetHideSold(bool bInHideSold);

	UFUNCTION(BlueprintPure, Category = "Configurator|Filter")
	bool IsHideSold() const { return bHideSold; }

	/** Можно ли выбрать квартиру с учётом фильтра (проданные при включённом фильтре — нельзя). */
	bool IsApartmentSelectable(const FApartmentData& Apartment) const;

	// ---------------------------------------------------------------- Сцена

	/** Регистрирует объект, реализующий IConfiguratorSceneProvider (обычно — актор здания). */
	void RegisterSceneProvider(UObject* ProviderObject);
	void UnregisterSceneProvider(UObject* ProviderObject);

	/** Текущий провайдер геометрии или nullptr, если здание ещё не построено. */
	IConfiguratorSceneProvider* GetSceneProvider() const;

	/**
	 * Здание перестроено по новым данным: история очищается, вид сбрасывается на общий план.
	 * Вызывается сценой, а не по событию загрузки данных, — так камера гарантированно
	 * получает уже готовую геометрию.
	 */
	void NotifySceneRebuilt();

	// ---------------------------------------------------------------- События

	UPROPERTY(BlueprintAssignable, Category = "Configurator|Events")
	FOnConfiguratorViewChanged OnViewChanged;

	UPROPERTY(BlueprintAssignable, Category = "Configurator|Events")
	FOnConfiguratorFilterChanged OnFilterChanged;

protected:
	/** Подсистема нужна только в игровых мирах (игра и PIE), но не в мирах редактора и превью. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Переход с записью текущего состояния в историю. */
	void NavigateTo(const FConfiguratorViewState& NewView);

	void SetCurrentView(const FConfiguratorViewState& NewView, EConfiguratorViewChangeReason Reason);

	/** Проверяет, что снимок из истории всё ещё валиден для текущих данных. */
	bool IsViewValid(const FConfiguratorViewState& View) const;

	UApartmentDataSubsystem* GetDataSubsystem() const;

	/** Глубина истории ограничена, чтобы долгий сеанс не копил память бесконечно. */
	static constexpr int32 MaxHistoryDepth = 32;

	FConfiguratorViewState CurrentView;
	TArray<FConfiguratorViewState> History;
	bool bHideSold = false;

	TWeakObjectPtr<UObject> SceneProviderObject;
};
