#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConfiguratorDataTypes.h"
#include "ApartmentCardWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;

DECLARE_DELEGATE(FOnApartmentCardCloseRequested);

/**
 * Карточка квартиры: ID, этаж, площадь, статус и кнопка «Забронировать».
 *
 * Карточка пассивна: она только отображает переданные данные и сообщает о нажатиях.
 * Когда её показывать, решает HUD по событию смены вида.
 */
UCLASS()
class APARTMENTCONFIGURATOR_API UApartmentCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Заполнить карточку данными квартиры. */
	void ShowApartment(const FApartmentData& Apartment, const FText& FloorName);

	/** Нажат крестик — HUD решает, куда вернуться. */
	FOnApartmentCardCloseRequested OnCloseRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UFUNCTION()
	void HandleBookClicked();

	UFUNCTION()
	void HandleCloseClicked();

private:
	void EnsureWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FloorText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AreaText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	/** Цветная «таблетка» под текстом статуса. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> StatusPill;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FeedbackText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> BookButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	/** Копия отображаемых данных — для сообщения о бронировании. */
	FApartmentData CurrentApartment;
};
