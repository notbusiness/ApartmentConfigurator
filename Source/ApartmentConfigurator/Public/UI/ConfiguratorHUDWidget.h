#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/ConfiguratorViewTypes.h"
#include "ConfiguratorHUDWidget.generated.h"

class UApartmentCardWidget;
class UApartmentDataSubsystem;
class UButton;
class UCheckBox;
class UConfiguratorListButton;
class UConfiguratorStateSubsystem;
class UTextBlock;
class UVerticalBox;
class UWrapBox;

/**
 * Главный интерфейс конфигуратора.
 *
 *  - Панель этажей: кнопки создаются по данным JSON (при каждой перезагрузке данных).
 *  - Список квартир выбранного этажа: цвет по статусу; при фильтре «Скрыть проданные»
 *    кнопки проданных становятся неактивными.
 *  - Карточка квартиры (UApartmentCardWidget) показывается в режиме Apartment Focus.
 *  - Кнопки «Общий вид», «Назад», «Обновить JSON», строка статуса загрузки.
 *  - Строка текущего режима сверху («Общий вид › этаж › квартира»).
 *
 * HUD — «тонкий» слой представления: хранит только ссылки на свои виджеты,
 * команды отправляет в UConfiguratorStateSubsystem, а перерисовывается по его событиям.
 * Все обновления точечные и событийные — у виджета нет NativeTick.
 */
UCLASS()
class APARTMENTCONFIGURATOR_API UConfiguratorHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// ---------------------------------------------------------------- События моделей

	UFUNCTION()
	void HandleDataLoaded();

	UFUNCTION()
	void HandleLoadFailed(const FString& ErrorMessage);

	UFUNCTION()
	void HandleViewChanged(const FConfiguratorViewState& NewView, EConfiguratorViewChangeReason Reason);

	UFUNCTION()
	void HandleFilterChanged(bool bHideSold);

	// ---------------------------------------------------------------- Действия пользователя

	UFUNCTION()
	void HandleHideSoldToggled(bool bIsChecked);

	UFUNCTION()
	void HandleGenplanClicked();

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleReloadClicked();

	void HandleFloorButtonClicked(UConfiguratorListButton* Button);
	void HandleApartmentButtonClicked(UConfiguratorListButton* Button);
	void HandleCardCloseRequested();

private:
	void EnsureWidgetTree();
	void BindToSubsystems();

	void RebuildFloorList();
	void RebuildApartmentList(int32 FloorIndex);

	void RefreshFloorSelection();
	void RefreshApartmentButtons();
	void RefreshCard();
	void RefreshNavigationButtons();
	void RefreshBreadcrumb();

	void SetStatus(const FText& Text, const FLinearColor& Color);

	UConfiguratorListButton* CreateListButton();

	// ---------------------------------------------------------------- Виджеты

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> GenplanButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> BackButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ReloadButton;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> HideSoldCheckBox;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> FloorList;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ApartmentHeaderText;

	/** Подсказка вместо списка квартир («выберите этаж» / «квартир нет»). */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ApartmentHintText;

	UPROPERTY(Transient)
	TObjectPtr<UWrapBox> ApartmentList;

	/** Строка текущего режима сверху: «Общий вид › этаж › квартира». */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BreadcrumbText;

	UPROPERTY(Transient)
	TObjectPtr<UApartmentCardWidget> ApartmentCard;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UConfiguratorListButton>> FloorButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UConfiguratorListButton>> ApartmentButtons;

	/** Этаж, квартиры которого сейчас показаны в списке (чтобы не пересоздавать список без нужды). */
	int32 DisplayedApartmentFloor = INDEX_NONE;

	TWeakObjectPtr<UApartmentDataSubsystem> DataSubsystem;
	TWeakObjectPtr<UConfiguratorStateSubsystem> StateSubsystem;
};
