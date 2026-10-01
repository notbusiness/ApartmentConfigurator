#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConfiguratorListButton.generated.h"

class UButton;
class UConfiguratorListButton;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FOnConfiguratorListButtonClicked, UConfiguratorListButton* /*Button*/);

/** Раскладка содержимого кнопки списка. */
enum class EConfiguratorListButtonLayout : uint8
{
	/** Строка на всю ширину панели: основная подпись слева, дополнительная — справа (этажи). */
	Row,
	/** Компактная плитка: основная подпись над дополнительной (квартиры). */
	Chip
};

/**
 * Кнопка динамического списка (этаж или квартира) с полезной нагрузкой.
 *
 * Стандартный UButton::OnClicked не передаёт параметров, поэтому для кнопок, созданных
 * по данным, нужен носитель «какой этаж / какая квартира». Кнопка хранит индекс и строку
 * и сообщает о нажатии через нативный делегат с указателем на себя.
 *
 * Дерево виджетов собирается в C++ — проекту не нужен WBP-ассет.
 */
UCLASS()
class APARTMENTCONFIGURATOR_API UConfiguratorListButton : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(EConfiguratorListButtonLayout InLayout, const FText& Primary, const FText& Secondary, int32 InIntPayload, const FString& InStringPayload);

	/** Базовый цвет кнопки (например, по статусу квартиры). */
	void SetBaseColor(const FLinearColor& InColor);

	/** Выделение текущего элемента (активный этаж / выбранная квартира). */
	void SetSelected(bool bInSelected);

	int32 GetIntPayload() const { return IntPayload; }
	const FString& GetStringPayload() const { return StringPayload; }

	FOnConfiguratorListButtonClicked OnClickedWithPayload;

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UFUNCTION()
	void HandleClicked();

private:
	/** Создаёт корневую кнопку при первом обращении (идемпотентно). */
	void EnsureWidgetTree();
	void ApplyColors();

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PrimaryText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SecondaryText;

	int32 IntPayload = INDEX_NONE;
	FString StringPayload;

	EConfiguratorListButtonLayout Layout = EConfiguratorListButtonLayout::Row;

	FLinearColor BaseColor = FLinearColor(0.02f, 0.025f, 0.04f);
	bool bSelected = false;
};
