#include "UI/ConfiguratorHUDWidget.h"

#include "ApartmentDataSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Core/ConfiguratorStateSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UI/ApartmentCardWidget.h"
#include "UI/ConfiguratorButton.h"
#include "UI/ConfiguratorListButton.h"
#include "UI/ConfiguratorUIStyle.h"

#define LOCTEXT_NAMESPACE "ConfiguratorHUD"

namespace
{
	FText FormatArea(float Area)
	{
		FNumberFormattingOptions Format;
		Format.MinimumFractionalDigits = 0;
		Format.MaximumFractionalDigits = 1;
		return FText::AsNumber(Area, &Format);
	}
}

// ---------------------------------------------------------------- Жизненный цикл

void UConfiguratorHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Пустые области HUD должны пропускать клики в 3D-сцену; клики ловят только панели и кнопки.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// Дерево строится один раз за жизнь виджета.
	EnsureWidgetTree();
}

void UConfiguratorHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Подписка — при каждом появлении на экране (парная отписке в NativeDestruct),
	// поэтому HUD, убранный и снова добавленный во viewport, продолжает получать события.
	BindToSubsystems();
}

TSharedRef<SWidget> UConfiguratorHUDWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UConfiguratorHUDWidget::NativeDestruct()
{
	if (UApartmentDataSubsystem* Data = DataSubsystem.Get())
	{
		Data->OnDataLoaded.RemoveDynamic(this, &UConfiguratorHUDWidget::HandleDataLoaded);
		Data->OnLoadFailed.RemoveDynamic(this, &UConfiguratorHUDWidget::HandleLoadFailed);
	}
	if (UConfiguratorStateSubsystem* State = StateSubsystem.Get())
	{
		State->OnViewChanged.RemoveDynamic(this, &UConfiguratorHUDWidget::HandleViewChanged);
		State->OnFilterChanged.RemoveDynamic(this, &UConfiguratorHUDWidget::HandleFilterChanged);
	}

	Super::NativeDestruct();
}

void UConfiguratorHUDWidget::BindToSubsystems()
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = GetGameInstance();
	StateSubsystem = World ? World->GetSubsystem<UConfiguratorStateSubsystem>() : nullptr;
	DataSubsystem = GameInstance ? GameInstance->GetSubsystem<UApartmentDataSubsystem>() : nullptr;

	if (UConfiguratorStateSubsystem* State = StateSubsystem.Get())
	{
		State->OnViewChanged.AddUniqueDynamic(this, &UConfiguratorHUDWidget::HandleViewChanged);
		State->OnFilterChanged.AddUniqueDynamic(this, &UConfiguratorHUDWidget::HandleFilterChanged);
		HideSoldCheckBox->SetIsChecked(State->IsHideSold());
	}

	if (UApartmentDataSubsystem* Data = DataSubsystem.Get())
	{
		Data->OnDataLoaded.AddUniqueDynamic(this, &UConfiguratorHUDWidget::HandleDataLoaded);
		Data->OnLoadFailed.AddUniqueDynamic(this, &UConfiguratorHUDWidget::HandleLoadFailed);

		// HUD может появиться позже загрузки — сразу отражаем текущее состояние.
		if (Data->HasData())
		{
			HandleDataLoaded();
		}
		else if (Data->GetLoadState() == EConfiguratorLoadState::Failed)
		{
			HandleLoadFailed(Data->GetLastError());
		}
	}

	RefreshNavigationButtons();
	RefreshCard();
}

// ---------------------------------------------------------------- Построение дерева виджетов

void UConfiguratorHUDWidget::EnsureWidgetTree()
{
	using namespace ConfiguratorUI;

	if (!WidgetTree || TitleText)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Canvas;

	// ================================================================ Левая панель
	USizeBox* LeftSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LeftSize->SetWidthOverride(340.f);
	UBorder* LeftPanel = MakePanel(*WidgetTree, FMargin(18.f));
	LeftSize->AddChild(LeftPanel);
	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	LeftPanel->AddChild(Left);

	if (UCanvasPanelSlot* LeftSlot = Canvas->AddChildToCanvas(LeftSize))
	{
		LeftSlot->SetAnchors(FAnchors(0.f, 0.f));
		LeftSlot->SetAlignment(FVector2D(0.f, 0.f));
		LeftSlot->SetPosition(FVector2D(20.f, 20.f));
		LeftSlot->SetAutoSize(true);
	}

	// ---- Шапка: название объекта из JSON.
	Left->AddChildToVerticalBox(MakeCaption(*WidgetTree, LOCTEXT("AppCaption", "КОНФИГУРАТОР КВАРТИР")));
	TitleText = MakeText(*WidgetTree, LOCTEXT("DefaultTitle", "Жилой комплекс"), 18, TextColor, true);
	Left->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(0.f, 4.f, 0.f, 14.f));

	// ---- Навигация: две кнопки равной ширины (делят строку пополам и не вылезают за панель).
	UHorizontalBox* NavRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	GenplanButton = MakeTextButton(*WidgetTree, LOCTEXT("Genplan", "Общий вид"), SurfaceColor);
	BackButton = MakeTextButton(*WidgetTree, LOCTEXT("Back", "\u2039  Назад"), SurfaceColor);
	GenplanButton->OnClicked.AddUniqueDynamic(this, &UConfiguratorHUDWidget::HandleGenplanClicked);
	BackButton->OnClicked.AddUniqueDynamic(this, &UConfiguratorHUDWidget::HandleBackClicked);
	if (UHorizontalBoxSlot* GenplanSlot = NavRow->AddChildToHorizontalBox(GenplanButton))
	{
		GenplanSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		GenplanSlot->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
	}
	if (UHorizontalBoxSlot* BackSlot = NavRow->AddChildToHorizontalBox(BackButton))
	{
		BackSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		BackSlot->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f));
	}
	Left->AddChildToVerticalBox(NavRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
	Left->AddChildToVerticalBox(MakeDivider(*WidgetTree));

	// ---- Фильтр.
	UHorizontalBox* FilterRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HideSoldCheckBox = WidgetTree->ConstructWidget<UConfiguratorCheckBox>(UConfiguratorCheckBox::StaticClass(), TEXT("HideSoldCheckBox"));
	StyleCheckBox(*HideSoldCheckBox);
	HideSoldCheckBox->OnCheckStateChanged.AddUniqueDynamic(this, &UConfiguratorHUDWidget::HandleHideSoldToggled);
	FilterRow->AddChildToHorizontalBox(HideSoldCheckBox)->SetVerticalAlignment(VAlign_Center);
	UTextBlock* FilterLabel = MakeText(*WidgetTree, LOCTEXT("HideSold", "Скрыть проданные"), 13, TextColor);
	if (UHorizontalBoxSlot* LabelSlot = FilterRow->AddChildToHorizontalBox(FilterLabel))
	{
		LabelSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}
	Left->AddChildToVerticalBox(FilterRow)->SetPadding(FMargin(0.f, 14.f, 0.f, 10.f));

	// ---- Легенда: цветная точка + подпись.
	UHorizontalBox* Legend = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	auto AddLegendItem = [this, Legend](const FText& Label, const FLinearColor& Color)
	{
		USizeBox* Dot = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Dot->SetWidthOverride(9.f);
		Dot->SetHeightOverride(9.f);
		Dot->AddChild(MakeRoundedBox(*WidgetTree, Color, 4.5f, FMargin(0.f)));
		Legend->AddChildToHorizontalBox(Dot)->SetVerticalAlignment(VAlign_Center);

		UTextBlock* Text = MakeText(*WidgetTree, Label, 11, MutedTextColor);
		Text->SetAutoWrapText(false);
		if (UHorizontalBoxSlot* TextSlot = Legend->AddChildToHorizontalBox(Text))
		{
			TextSlot->SetPadding(FMargin(6.f, 0.f, 14.f, 0.f));
			TextSlot->SetVerticalAlignment(VAlign_Center);
		}
	};
	AddLegendItem(LOCTEXT("LegendAvailable", "Свободно"), AvailableColor);
	AddLegendItem(LOCTEXT("LegendSold", "Продано"), SoldColor);
	AddLegendItem(LOCTEXT("LegendSelected", "Выбрано"), AccentColor);
	Left->AddChildToVerticalBox(Legend)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
	Left->AddChildToVerticalBox(MakeDivider(*WidgetTree));

	// ---- Этажи (верхний этаж — сверху, как в лифте). Список прокручивается, если этажей много.
	Left->AddChildToVerticalBox(MakeCaption(*WidgetTree, LOCTEXT("Floors", "ЭТАЖИ")))->SetPadding(FMargin(0.f, 14.f, 0.f, 8.f));
	USizeBox* FloorScrollSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	FloorScrollSize->SetMaxDesiredHeight(330.f);
	UScrollBox* FloorScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	FloorScrollSize->AddChild(FloorScroll);
	FloorList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FloorList"));
	FloorScroll->AddChild(FloorList);
	Left->AddChildToVerticalBox(FloorScrollSize);

	// ---- Квартиры выбранного этажа.
	ApartmentHeaderText = MakeCaption(*WidgetTree, FText::GetEmpty());
	Left->AddChildToVerticalBox(ApartmentHeaderText)->SetPadding(FMargin(0.f, 12.f, 0.f, 8.f));
	ApartmentHintText = MakeText(*WidgetTree, FText::GetEmpty(), 12, MutedTextColor);
	Left->AddChildToVerticalBox(ApartmentHintText);
	ApartmentList = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("ApartmentList"));
	ApartmentList->SetInnerSlotPadding(FVector2D(6.f, 6.f));
	Left->AddChildToVerticalBox(ApartmentList);

	// ---- Данные: статус загрузки и перезагрузка файла.
	Left->AddChildToVerticalBox(MakeDivider(*WidgetTree))->SetPadding(FMargin(0.f, 16.f, 0.f, 12.f));
	UHorizontalBox* DataRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	StatusText = MakeText(*WidgetTree, LOCTEXT("Loading", "Загрузка данных…"), 10, MutedTextColor);
	if (UHorizontalBoxSlot* StatusSlot = DataRow->AddChildToHorizontalBox(StatusText))
	{
		StatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		StatusSlot->SetVerticalAlignment(VAlign_Center);
		StatusSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	}
	ReloadButton = MakeTextButton(*WidgetTree, LOCTEXT("Reload", "Обновить JSON"), SurfaceColor, nullptr, 10, FMargin(10.f, 6.f));
	ReloadButton->OnClicked.AddUniqueDynamic(this, &UConfiguratorHUDWidget::HandleReloadClicked);
	DataRow->AddChildToHorizontalBox(ReloadButton)->SetVerticalAlignment(VAlign_Center);
	Left->AddChildToVerticalBox(DataRow);

	// ================================================================ Текущий режим (сверху по центру)
	UBorder* BreadcrumbPanel = MakePanel(*WidgetTree, FMargin(18.f, 8.f));
	BreadcrumbText = MakeText(*WidgetTree, LOCTEXT("ModeGenplan", "Общий вид"), 12, TextColor, true);
	BreadcrumbText->SetAutoWrapText(false);
	BreadcrumbPanel->AddChild(BreadcrumbText);
	BreadcrumbPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* BreadcrumbSlot = Canvas->AddChildToCanvas(BreadcrumbPanel))
	{
		BreadcrumbSlot->SetAnchors(FAnchors(0.5f, 0.f));
		BreadcrumbSlot->SetAlignment(FVector2D(0.5f, 0.f));
		BreadcrumbSlot->SetPosition(FVector2D(0.f, 20.f));
		BreadcrumbSlot->SetAutoSize(true);
	}

	// ================================================================ Карточка квартиры (справа)
	ApartmentCard = WidgetTree->ConstructWidget<UApartmentCardWidget>(UApartmentCardWidget::StaticClass(), TEXT("ApartmentCard"));
	ApartmentCard->OnCloseRequested.BindUObject(this, &UConfiguratorHUDWidget::HandleCardCloseRequested);
	ApartmentCard->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* CardSlot = Canvas->AddChildToCanvas(ApartmentCard))
	{
		CardSlot->SetAnchors(FAnchors(1.f, 0.f));
		CardSlot->SetAlignment(FVector2D(1.f, 0.f));
		CardSlot->SetPosition(FVector2D(-20.f, 20.f));
		CardSlot->SetAutoSize(true);
	}

	// ================================================================ Подсказка по управлению (снизу)
	UBorder* HintPanel = MakePanel(*WidgetTree, FMargin(16.f, 7.f));
	UTextBlock* Hint = MakeText(*WidgetTree,
		LOCTEXT("Hint", "ЛКМ — выбрать квартиру   ·   ЛКМ/ПКМ + движение — вращение   ·   Колесо — зум   ·   Backspace — назад"),
		11, MutedTextColor);
	Hint->SetAutoWrapText(false);
	HintPanel->AddChild(Hint);
	HintPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* HintSlot = Canvas->AddChildToCanvas(HintPanel))
	{
		HintSlot->SetAnchors(FAnchors(0.5f, 1.f));
		HintSlot->SetAlignment(FVector2D(0.5f, 1.f));
		HintSlot->SetPosition(FVector2D(0.f, -16.f));
		HintSlot->SetAutoSize(true);
	}

	RebuildApartmentList(INDEX_NONE);
}

UConfiguratorListButton* UConfiguratorHUDWidget::CreateListButton()
{
	return WidgetTree->ConstructWidget<UConfiguratorListButton>(UConfiguratorListButton::StaticClass());
}

// ---------------------------------------------------------------- События данных и состояния

void UConfiguratorHUDWidget::HandleDataLoaded()
{
	const UApartmentDataSubsystem* Data = DataSubsystem.Get();
	if (!Data)
	{
		return;
	}

	const FBuildingData& Building = Data->GetBuildingData();
	TitleText->SetText(Building.Name.IsEmpty() ? LOCTEXT("DefaultTitle", "Жилой комплекс") : FText::FromString(Building.Name));

	const int32 WarningCount = Data->GetLastWarnings().Num();
	FText Status = FText::Format(LOCTEXT("Loaded", "Этажей: {0}  ·  квартир: {1}"), Building.Floors.Num(), Building.GetApartmentCount());
	if (WarningCount > 0)
	{
		Status = FText::Format(LOCTEXT("LoadedWithWarnings", "{0}\nИсправлено полей: {1} (см. Output Log)"), Status, WarningCount);
	}
	SetStatus(Status, WarningCount > 0 ? ConfiguratorUI::AccentColor : ConfiguratorUI::MutedTextColor);

	RebuildFloorList();

	const UConfiguratorStateSubsystem* State = StateSubsystem.Get();
	const FConfiguratorViewState View = State ? State->GetCurrentViewRef() : FConfiguratorViewState();
	RebuildApartmentList(View.Mode == EConfiguratorViewMode::Genplan ? INDEX_NONE : View.FloorIndex);

	RefreshFloorSelection();
	RefreshApartmentButtons();
	RefreshCard();
	RefreshNavigationButtons();
	RefreshBreadcrumb();
}

void UConfiguratorHUDWidget::HandleLoadFailed(const FString& ErrorMessage)
{
	const UApartmentDataSubsystem* Data = DataSubsystem.Get();
	const FText Status = (Data && Data->HasData())
		? FText::Format(LOCTEXT("LoadFailedKeep", "Ошибка загрузки: {0}. Показаны прежние данные."), FText::FromString(ErrorMessage))
		: FText::Format(LOCTEXT("LoadFailed", "Ошибка загрузки: {0}"), FText::FromString(ErrorMessage));
	SetStatus(Status, ConfiguratorUI::SoldColor);
}

void UConfiguratorHUDWidget::HandleViewChanged(const FConfiguratorViewState& NewView, EConfiguratorViewChangeReason Reason)
{
	// Список квартир пересоздаётся только при смене этажа; смена выбранной квартиры — это перекраска.
	const int32 FloorToShow = NewView.Mode == EConfiguratorViewMode::Genplan ? INDEX_NONE : NewView.FloorIndex;
	if (FloorToShow != DisplayedApartmentFloor || Reason == EConfiguratorViewChangeReason::Reset)
	{
		RebuildApartmentList(FloorToShow);
	}

	RefreshFloorSelection();
	RefreshApartmentButtons();
	RefreshCard();
	RefreshNavigationButtons();
	RefreshBreadcrumb();
}

void UConfiguratorHUDWidget::HandleFilterChanged(bool bHideSold)
{
	if (HideSoldCheckBox->IsChecked() != bHideSold)
	{
		HideSoldCheckBox->SetIsChecked(bHideSold);
	}
	RefreshApartmentButtons();
}

// ---------------------------------------------------------------- Действия пользователя

void UConfiguratorHUDWidget::HandleHideSoldToggled(bool bIsChecked)
{
	if (UConfiguratorStateSubsystem* State = StateSubsystem.Get())
	{
		State->SetHideSold(bIsChecked);
	}
}

void UConfiguratorHUDWidget::HandleGenplanClicked()
{
	if (UConfiguratorStateSubsystem* State = StateSubsystem.Get())
	{
		State->ShowGenplan();
	}
}

void UConfiguratorHUDWidget::HandleBackClicked()
{
	if (UConfiguratorStateSubsystem* State = StateSubsystem.Get())
	{
		State->GoBack();
	}
}

void UConfiguratorHUDWidget::HandleReloadClicked()
{
	if (UApartmentDataSubsystem* Data = DataSubsystem.Get())
	{
		SetStatus(LOCTEXT("Loading", "Загрузка данных…"), ConfiguratorUI::MutedTextColor);
		Data->RequestLoad();
	}
}

void UConfiguratorHUDWidget::HandleFloorButtonClicked(UConfiguratorListButton* Button)
{
	if (UConfiguratorStateSubsystem* State = StateSubsystem.Get())
	{
		State->ShowFloor(Button->GetIntPayload());
	}
}

void UConfiguratorHUDWidget::HandleApartmentButtonClicked(UConfiguratorListButton* Button)
{
	if (UConfiguratorStateSubsystem* State = StateSubsystem.Get())
	{
		State->ShowApartment(Button->GetStringPayload());
	}
}

void UConfiguratorHUDWidget::HandleCardCloseRequested()
{
	// Закрыть карточку = вернуться к виду этажа этой квартиры.
	if (UConfiguratorStateSubsystem* State = StateSubsystem.Get())
	{
		State->ShowFloor(State->GetCurrentViewRef().FloorIndex);
	}
}

// ---------------------------------------------------------------- Перестроение и обновление

void UConfiguratorHUDWidget::RebuildFloorList()
{
	FloorList->ClearChildren();
	FloorButtons.Reset();

	const UApartmentDataSubsystem* Data = DataSubsystem.Get();
	if (!Data || !Data->HasData())
	{
		return;
	}

	const TArray<FFloorData>& Floors = Data->GetBuildingData().Floors;
	for (int32 FloorIndex = Floors.Num() - 1; FloorIndex >= 0; --FloorIndex)
	{
		const FFloorData& Floor = Floors[FloorIndex];

		int32 AvailableCount = 0;
		for (const FApartmentData& Apartment : Floor.Apartments)
		{
			AvailableCount += Apartment.IsAvailable() ? 1 : 0;
		}

		UConfiguratorListButton* Button = CreateListButton();
		Button->Setup(EConfiguratorListButtonLayout::Row,
			FText::FromString(Floor.Name),
			FText::Format(LOCTEXT("FloorAvailability", "свободно {0} из {1}"), AvailableCount, Floor.Apartments.Num()),
			FloorIndex, FString());
		Button->OnClickedWithPayload.BindUObject(this, &UConfiguratorHUDWidget::HandleFloorButtonClicked);

		FloorList->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		FloorButtons.Add(Button);
	}
}

void UConfiguratorHUDWidget::RebuildApartmentList(int32 FloorIndex)
{
	DisplayedApartmentFloor = FloorIndex;
	ApartmentList->ClearChildren();
	ApartmentButtons.Reset();

	const UApartmentDataSubsystem* Data = DataSubsystem.Get();
	if (!Data || !Data->GetBuildingData().Floors.IsValidIndex(FloorIndex))
	{
		ApartmentHeaderText->SetText(LOCTEXT("ApartmentsCaption", "КВАРТИРЫ"));
		ApartmentHintText->SetText(LOCTEXT("PickFloor", "Выберите этаж, чтобы увидеть квартиры"));
		ApartmentHintText->SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}

	const FFloorData& Floor = Data->GetBuildingData().Floors[FloorIndex];
	ApartmentHeaderText->SetText(FText::Format(LOCTEXT("ApartmentsHeader", "КВАРТИРЫ  ·  {0}"), FText::FromString(Floor.Name).ToUpper()));

	if (Floor.Apartments.IsEmpty())
	{
		ApartmentHintText->SetText(LOCTEXT("NoApartments", "На этом этаже нет квартир"));
		ApartmentHintText->SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}
	ApartmentHintText->SetVisibility(ESlateVisibility::Collapsed);

	for (const FApartmentData& Apartment : Floor.Apartments)
	{
		const FText AreaLabel = Apartment.Area > 0.f
			? FText::Format(LOCTEXT("ApartmentArea", "{0} м²"), FormatArea(Apartment.Area))
			: LOCTEXT("ApartmentAreaUnknown", "—");

		UConfiguratorListButton* Button = CreateListButton();
		Button->Setup(EConfiguratorListButtonLayout::Chip, FText::FromString(Apartment.Id), AreaLabel, FloorIndex, Apartment.Id);
		Button->OnClickedWithPayload.BindUObject(this, &UConfiguratorHUDWidget::HandleApartmentButtonClicked);

		ApartmentList->AddChildToWrapBox(Button);
		ApartmentButtons.Add(Button);
	}
}

void UConfiguratorHUDWidget::RefreshFloorSelection()
{
	const UConfiguratorStateSubsystem* State = StateSubsystem.Get();
	const FConfiguratorViewState View = State ? State->GetCurrentViewRef() : FConfiguratorViewState();
	const int32 ActiveFloor = View.Mode == EConfiguratorViewMode::Genplan ? INDEX_NONE : View.FloorIndex;

	for (UConfiguratorListButton* Button : FloorButtons)
	{
		Button->SetSelected(Button->GetIntPayload() == ActiveFloor);
	}
}

void UConfiguratorHUDWidget::RefreshApartmentButtons()
{
	using namespace ConfiguratorUI;

	const UApartmentDataSubsystem* Data = DataSubsystem.Get();
	const UConfiguratorStateSubsystem* State = StateSubsystem.Get();
	if (!Data)
	{
		return;
	}

	const FConfiguratorViewState View = State ? State->GetCurrentViewRef() : FConfiguratorViewState();

	for (UConfiguratorListButton* Button : ApartmentButtons)
	{
		const FApartmentData* Apartment = Data->FindApartment(Button->GetStringPayload());
		if (!Apartment)
		{
			Button->SetIsEnabled(false);
			continue;
		}

		Button->SetBaseColor(Apartment->IsAvailable() ? AvailableChipColor : (Apartment->IsSold() ? SoldChipColor : UnknownChipColor));

		// Фильтр «Скрыть проданные»: кнопки проданных неактивны (и в сцене они затемнены).
		Button->SetIsEnabled(!State || State->IsApartmentSelectable(*Apartment));
		Button->SetSelected(View.Mode == EConfiguratorViewMode::Apartment && View.ApartmentId == Apartment->Id);
	}
}

void UConfiguratorHUDWidget::RefreshCard()
{
	const UApartmentDataSubsystem* Data = DataSubsystem.Get();
	const UConfiguratorStateSubsystem* State = StateSubsystem.Get();

	if (Data && State && State->GetCurrentViewRef().Mode == EConfiguratorViewMode::Apartment)
	{
		int32 FloorIndex = INDEX_NONE;
		if (const FApartmentData* Apartment = Data->FindApartment(State->GetCurrentViewRef().ApartmentId, &FloorIndex))
		{
			const TArray<FFloorData>& Floors = Data->GetBuildingData().Floors;
			const FText FloorName = Floors.IsValidIndex(FloorIndex) ? FText::FromString(Floors[FloorIndex].Name) : FText::GetEmpty();

			ApartmentCard->ShowApartment(*Apartment, FloorName);
			ApartmentCard->SetVisibility(ESlateVisibility::Visible);
			return;
		}
	}

	ApartmentCard->SetVisibility(ESlateVisibility::Collapsed);
}

void UConfiguratorHUDWidget::RefreshNavigationButtons()
{
	const UConfiguratorStateSubsystem* State = StateSubsystem.Get();
	BackButton->SetIsEnabled(State && State->CanGoBack());
	GenplanButton->SetIsEnabled(State && State->GetCurrentViewRef().Mode != EConfiguratorViewMode::Genplan);
}

void UConfiguratorHUDWidget::RefreshBreadcrumb()
{
	// «Общий вид › 4 этаж › Квартира 403» — пользователь всегда видит, в каком режиме находится.
	const UApartmentDataSubsystem* Data = DataSubsystem.Get();
	const UConfiguratorStateSubsystem* State = StateSubsystem.Get();
	const FConfiguratorViewState View = State ? State->GetCurrentViewRef() : FConfiguratorViewState();

	FText Path = LOCTEXT("ModeGenplan", "Общий вид");
	if (View.Mode != EConfiguratorViewMode::Genplan && Data && Data->GetBuildingData().Floors.IsValidIndex(View.FloorIndex))
	{
		Path = FText::Format(LOCTEXT("ModeFloor", "{0}   \u203A   {1}"), Path, FText::FromString(Data->GetBuildingData().Floors[View.FloorIndex].Name));
		if (View.Mode == EConfiguratorViewMode::Apartment)
		{
			Path = FText::Format(LOCTEXT("ModeApartment", "{0}   \u203A   Квартира {1}"), Path, FText::FromString(View.ApartmentId));
		}
	}
	BreadcrumbText->SetText(Path);
}

void UConfiguratorHUDWidget::SetStatus(const FText& Text, const FLinearColor& Color)
{
	StatusText->SetText(Text);
	StatusText->SetColorAndOpacity(FSlateColor(Color));
}

#undef LOCTEXT_NAMESPACE
