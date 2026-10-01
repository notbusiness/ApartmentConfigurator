#include "UI/ApartmentCardWidget.h"

#include "ApartmentConfigurator.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/ConfiguratorUIStyle.h"

#define LOCTEXT_NAMESPACE "ApartmentCard"

void UApartmentCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWidgetTree();
}

TSharedRef<SWidget> UApartmentCardWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UApartmentCardWidget::EnsureWidgetTree()
{
	using namespace ConfiguratorUI;

	if (!WidgetTree || TitleText)
	{
		return;
	}

	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CardRoot"));
	Root->SetWidthOverride(340.f);

	UBorder* Panel = MakePanel(*WidgetTree, FMargin(20.f, 18.f));
	Root->AddChild(Panel);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->AddChild(Content);

	// Шапка: подпись секции и кнопка закрытия.
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UHorizontalBoxSlot* CaptionSlot = Header->AddChildToHorizontalBox(MakeCaption(*WidgetTree, LOCTEXT("Caption", "КВАРТИРА"))))
	{
		CaptionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CaptionSlot->SetVerticalAlignment(VAlign_Center);
	}
	CloseButton = MakeTextButton(*WidgetTree, FText::FromString(TEXT("×")), SurfaceColor, nullptr, 13, FMargin(9.f, 2.f));
	CloseButton->OnClicked.AddUniqueDynamic(this, &UApartmentCardWidget::HandleCloseClicked);
	Header->AddChildToHorizontalBox(CloseButton)->SetVerticalAlignment(VAlign_Center);
	Content->AddChildToVerticalBox(Header);

	// Номер квартиры и «таблетка» статуса.
	UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	TitleText = MakeText(*WidgetTree, FText::GetEmpty(), 28, TextColor, true);
	TitleText->SetAutoWrapText(false);
	if (UHorizontalBoxSlot* TitleSlot = TitleRow->AddChildToHorizontalBox(TitleText))
	{
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
	}
	StatusPill = MakeRoundedBox(*WidgetTree, UnknownChipColor, 11.f, FMargin(12.f, 4.f));
	StatusText = MakeText(*WidgetTree, FText::GetEmpty(), 11, TextColor, true);
	StatusText->SetAutoWrapText(false);
	StatusPill->AddChild(StatusText);
	TitleRow->AddChildToHorizontalBox(StatusPill)->SetVerticalAlignment(VAlign_Center);
	Content->AddChildToVerticalBox(TitleRow)->SetPadding(FMargin(0.f, 2.f, 0.f, 12.f));

	// Атрибуты: строки «подпись — значение» с разделителями.
	auto AddInfoRow = [this, Content](const FText& Label, TObjectPtr<UTextBlock>& OutValue)
	{
		Content->AddChildToVerticalBox(MakeDivider(*WidgetTree));

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(MakeText(*WidgetTree, Label, 12, MutedTextColor)))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		OutValue = MakeText(*WidgetTree, FText::GetEmpty(), 13, TextColor, true);
		OutValue->SetAutoWrapText(false);
		Row->AddChildToHorizontalBox(OutValue)->SetVerticalAlignment(VAlign_Center);

		Content->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 9.f));
	};
	AddInfoRow(LOCTEXT("FloorLabel", "Этаж"), FloorText);
	AddInfoRow(LOCTEXT("AreaLabel", "Площадь"), AreaText);
	Content->AddChildToVerticalBox(MakeDivider(*WidgetTree));

	// Главное действие.
	BookButton = MakeTextButton(*WidgetTree, LOCTEXT("Book", "Забронировать"), PrimaryColor, nullptr, 13, FMargin(12.f, 11.f));
	BookButton->OnClicked.AddUniqueDynamic(this, &UApartmentCardWidget::HandleBookClicked);
	Content->AddChildToVerticalBox(BookButton)->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));

	FeedbackText = MakeText(*WidgetTree, FText::GetEmpty(), 11, AvailableColor);
	FeedbackText->SetVisibility(ESlateVisibility::Collapsed);
	Content->AddChildToVerticalBox(FeedbackText)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));

	WidgetTree->RootWidget = Root;
}

void UApartmentCardWidget::ShowApartment(const FApartmentData& Apartment, const FText& FloorName)
{
	using namespace ConfiguratorUI;

	EnsureWidgetTree();
	CurrentApartment = Apartment;

	TitleText->SetText(FText::Format(LOCTEXT("Title", "№ {0}"), FText::FromString(Apartment.Id)));
	FloorText->SetText(FloorName);

	if (Apartment.Area > 0.f)
	{
		FNumberFormattingOptions Format;
		Format.MinimumFractionalDigits = 0;
		Format.MaximumFractionalDigits = 1;
		AreaText->SetText(FText::Format(LOCTEXT("Area", "{0} м²"), FText::AsNumber(Apartment.Area, &Format)));
	}
	else
	{
		AreaText->SetText(LOCTEXT("AreaUnknown", "не указана"));
	}

	const FLinearColor PillColor = Apartment.IsAvailable() ? AvailableChipColor : (Apartment.IsSold() ? SoldChipColor : UnknownChipColor);
	StatusPill->SetBrush(FSlateRoundedBoxBrush(PillColor, 11.f));
	StatusText->SetText(GetApartmentStatusText(Apartment.Status));

	// Забронировать можно только свободную квартиру.
	BookButton->SetIsEnabled(Apartment.IsAvailable());
	FeedbackText->SetVisibility(ESlateVisibility::Collapsed);
}

void UApartmentCardWidget::HandleBookClicked()
{
	UE_LOG(LogConfigurator, Display, TEXT("Booking requested: apartment '%s', area %.1f m2"),
		*CurrentApartment.Id, CurrentApartment.Area);

	FeedbackText->SetText(FText::Format(LOCTEXT("BookSent", "Заявка на квартиру {0} отправлена (см. Output Log)"),
		FText::FromString(CurrentApartment.Id)));
	FeedbackText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UApartmentCardWidget::HandleCloseClicked()
{
	OnCloseRequested.ExecuteIfBound();
}

#undef LOCTEXT_NAMESPACE
