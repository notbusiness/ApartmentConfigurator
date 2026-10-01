#include "UI/ConfiguratorListButton.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/ConfiguratorUIStyle.h"

void UConfiguratorListButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWidgetTree();
}

TSharedRef<SWidget> UConfiguratorListButton::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UConfiguratorListButton::EnsureWidgetTree()
{
	if (!WidgetTree || Button)
	{
		return;
	}

	BaseColor = ConfiguratorUI::SurfaceColor;
	Button = ConfiguratorUI::MakeButton(*WidgetTree, BaseColor);
	Button->OnClicked.AddUniqueDynamic(this, &UConfiguratorListButton::HandleClicked);
	WidgetTree->RootWidget = Button;
}

void UConfiguratorListButton::Setup(EConfiguratorListButtonLayout InLayout, const FText& Primary, const FText& Secondary, int32 InIntPayload, const FString& InStringPayload)
{
	using namespace ConfiguratorUI;

	EnsureWidgetTree();
	IntPayload = InIntPayload;
	StringPayload = InStringPayload;
	Layout = InLayout;

	if (!Button || PrimaryText)
	{
		return;
	}

	UWidget* Content = nullptr;
	FMargin ContentPadding;

	if (Layout == EConfiguratorListButtonLayout::Row)
	{
		// [Название этажа ...........  свободно 2 из 4]
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		PrimaryText = MakeText(*WidgetTree, Primary, 12, TextColor, true);
		// Название приходит из JSON и может быть длинным — переносим внутри своей колонки.
		if (UHorizontalBoxSlot* PrimarySlot = Row->AddChildToHorizontalBox(PrimaryText))
		{
			PrimarySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			PrimarySlot->SetVerticalAlignment(VAlign_Center);
		}

		SecondaryText = MakeText(*WidgetTree, Secondary, 11, MutedTextColor);
		SecondaryText->SetAutoWrapText(false);
		if (UHorizontalBoxSlot* SecondarySlot = Row->AddChildToHorizontalBox(SecondaryText))
		{
			SecondarySlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
			SecondarySlot->SetVerticalAlignment(VAlign_Center);
		}

		Content = Row;
		ContentPadding = FMargin(12.f, 8.f);
	}
	else
	{
		// Плитка: номер квартиры над площадью.
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		PrimaryText = MakeText(*WidgetTree, Primary, 13, TextColor, true);
		PrimaryText->SetAutoWrapText(false);
		PrimaryText->SetJustification(ETextJustify::Center);
		// Одинаковая минимальная ширина — плитки выстраиваются ровной сеткой.
		PrimaryText->SetMinDesiredWidth(58.f);
		Column->AddChildToVerticalBox(PrimaryText)->SetHorizontalAlignment(HAlign_Center);

		SecondaryText = MakeText(*WidgetTree, Secondary, 10, TextColor);
		SecondaryText->SetAutoWrapText(false);
		SecondaryText->SetJustification(ETextJustify::Center);
		SecondaryText->SetRenderOpacity(0.75f);
		Column->AddChildToVerticalBox(SecondaryText)->SetHorizontalAlignment(HAlign_Center);

		Content = Column;
		ContentPadding = FMargin(8.f, 6.f);
	}

	if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Button->AddChild(Content)))
	{
		ContentSlot->SetPadding(ContentPadding);
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
	}

	ApplyColors();
}

void UConfiguratorListButton::SetBaseColor(const FLinearColor& InColor)
{
	if (!BaseColor.Equals(InColor))
	{
		BaseColor = InColor;
		ApplyColors();
	}
}

void UConfiguratorListButton::SetSelected(bool bInSelected)
{
	if (bSelected != bInSelected)
	{
		bSelected = bInSelected;
		ApplyColors();
	}
}

void UConfiguratorListButton::ApplyColors()
{
	using namespace ConfiguratorUI;

	if (!Button)
	{
		return;
	}

	// Выбранный элемент — акцентный фон и тёмный текст (контраст лучше, чем у белого на жёлтом).
	StyleButton(*Button, bSelected ? AccentColor : BaseColor);

	if (PrimaryText)
	{
		PrimaryText->SetColorAndOpacity(FSlateColor(bSelected ? AccentTextColor : TextColor));
	}
	if (SecondaryText)
	{
		const bool bRow = Layout == EConfiguratorListButtonLayout::Row;
		SecondaryText->SetColorAndOpacity(FSlateColor(bSelected ? AccentTextColor : (bRow ? MutedTextColor : TextColor)));
	}
}

void UConfiguratorListButton::HandleClicked()
{
	OnClickedWithPayload.ExecuteIfBound(this);
}
