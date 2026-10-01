#include "UI/ConfiguratorUIStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CheckBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/SlateTypes.h"
#include "UI/ConfiguratorButton.h"

namespace ConfiguratorUI
{
	FLinearColor Hex(const TCHAR* HexCode, float Alpha)
	{
		FLinearColor Color = FLinearColor::FromSRGBColor(FColor::FromHex(HexCode));
		Color.A = Alpha;
		return Color;
	}

	const FLinearColor PanelColor = Hex(TEXT("#0F131A"), 0.90f);
	const FLinearColor PanelOutlineColor = Hex(TEXT("#FFFFFF"), 0.10f);
	const FLinearColor SurfaceColor = Hex(TEXT("#242C38"));
	const FLinearColor DividerColor = Hex(TEXT("#FFFFFF"), 0.08f);
	const FLinearColor TextColor = Hex(TEXT("#F2F4F7"));
	const FLinearColor MutedTextColor = Hex(TEXT("#94A0B0"));
	const FLinearColor AccentColor = Hex(TEXT("#F5A524"));
	const FLinearColor AccentTextColor = Hex(TEXT("#1B1400"));
	const FLinearColor PrimaryColor = Hex(TEXT("#16A34A"));
	const FLinearColor AvailableColor = Hex(TEXT("#34D399"));
	const FLinearColor SoldColor = Hex(TEXT("#F87171"));
	const FLinearColor UnknownColor = Hex(TEXT("#9CA3AF"));
	const FLinearColor AvailableChipColor = Hex(TEXT("#1D5C42"));
	const FLinearColor SoldChipColor = Hex(TEXT("#6B2B2E"));
	const FLinearColor UnknownChipColor = Hex(TEXT("#3B424D"));

	UTextBlock* MakeText(UWidgetTree& Tree, const FText& Text, int32 FontSize, const FLinearColor& Color, bool bBold)
	{
		UTextBlock* TextBlock = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TextBlock->SetText(Text);

		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		Font.TypefaceFontName = bBold ? TEXT("Bold") : TEXT("Regular");
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetAutoWrapText(true);
		return TextBlock;
	}

	UTextBlock* MakeCaption(UWidgetTree& Tree, const FText& Text)
	{
		UTextBlock* Caption = MakeText(Tree, Text, 9, MutedTextColor, true);
		FSlateFontInfo Font = Caption->GetFont();
		Font.LetterSpacing = 120;
		Caption->SetFont(Font);
		Caption->SetAutoWrapText(false);
		return Caption;
	}

	UBorder* MakeRoundedBox(UWidgetTree& Tree, const FLinearColor& Color, float Radius, const FMargin& Padding)
	{
		UBorder* Border = Tree.ConstructWidget<UBorder>(UBorder::StaticClass());
		Border->SetBrush(FSlateRoundedBoxBrush(Color, Radius));
		Border->SetBrushColor(FLinearColor::White);
		Border->SetPadding(Padding);
		return Border;
	}

	UBorder* MakePanel(UWidgetTree& Tree, const FMargin& Padding)
	{
		UBorder* Border = Tree.ConstructWidget<UBorder>(UBorder::StaticClass());
		Border->SetBrush(FSlateRoundedBoxBrush(PanelColor, PanelRadius, PanelOutlineColor, 1.f));
		Border->SetBrushColor(FLinearColor::White);
		Border->SetPadding(Padding);
		return Border;
	}

	UWidget* MakeDivider(UWidgetTree& Tree)
	{
		USizeBox* Size = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetHeightOverride(1.f);
		Size->AddChild(MakeRoundedBox(Tree, DividerColor, 0.f, FMargin(0.f)));
		return Size;
	}

	void StyleButton(UButton& Button, const FLinearColor& Color, float Radius, const FLinearColor& OutlineColor, float OutlineWidth)
	{
		// Производные состояния: наведение — светлее, нажатие — темнее, отключено — полупрозрачно.
		FLinearColor Hovered = FMath::Lerp(Color, FLinearColor::White, 0.14f);
		Hovered.A = Color.A;
		FLinearColor Pressed = Color * 0.82f;
		Pressed.A = Color.A;
		FLinearColor Disabled = Color;
		Disabled.A = Color.A * 0.35f;

		auto MakeBrush = [Radius, &OutlineColor, OutlineWidth](const FLinearColor& Fill)
		{
			return OutlineWidth > 0.f
				? FSlateRoundedBoxBrush(Fill, Radius, OutlineColor, OutlineWidth)
				: FSlateRoundedBoxBrush(Fill, Radius);
		};

		FButtonStyle Style;
		Style.SetNormal(MakeBrush(Color));
		Style.SetHovered(MakeBrush(Hovered));
		Style.SetPressed(MakeBrush(Pressed));
		Style.SetDisabled(MakeBrush(Disabled));
		Style.SetNormalPadding(FMargin(0.f));
		Style.SetPressedPadding(FMargin(0.f));

		Button.SetStyle(Style);
		Button.SetBackgroundColor(FLinearColor::White);
	}

	UButton* MakeButton(UWidgetTree& Tree, const FLinearColor& Color, float Radius)
	{
		// UConfiguratorButton не забирает фокус клавиатуры: иначе после клика Space/Enter
		// повторно «нажимали» бы последнюю кнопку.
		UButton* Button = Tree.ConstructWidget<UConfiguratorButton>(UConfiguratorButton::StaticClass());
		StyleButton(*Button, Color, Radius);
		return Button;
	}

	UButton* MakeTextButton(UWidgetTree& Tree, const FText& Text, const FLinearColor& Color, UTextBlock** OutLabel, int32 FontSize, const FMargin& Padding)
	{
		UButton* Button = MakeButton(Tree, Color);

		UTextBlock* Label = MakeText(Tree, Text, FontSize, TextColor, true);
		Label->SetAutoWrapText(false);
		Label->SetJustification(ETextJustify::Center);

		if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(Button->AddChild(Label)))
		{
			LabelSlot->SetPadding(Padding);
			LabelSlot->SetHorizontalAlignment(HAlign_Fill);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		if (OutLabel)
		{
			*OutLabel = Label;
		}
		return Button;
	}

	void StyleCheckBox(UCheckBox& CheckBox)
	{
		const FVector2f BoxSize(18.f, 18.f);
		const float Radius = 5.f;

		const FSlateRoundedBoxBrush Unchecked(SurfaceColor, Radius, MutedTextColor, 1.5f, BoxSize);
		const FSlateRoundedBoxBrush UncheckedHovered(SurfaceColor, Radius, TextColor, 1.5f, BoxSize);
		const FSlateRoundedBoxBrush Checked(AccentColor, Radius, TextColor, 2.5f, BoxSize);
		const FSlateRoundedBoxBrush CheckedHovered(FMath::Lerp(AccentColor, FLinearColor::White, 0.15f), Radius, TextColor, 2.5f, BoxSize);

		FCheckBoxStyle Style;
		Style.SetCheckBoxType(ESlateCheckBoxType::CheckBox);
		Style.SetUncheckedImage(Unchecked);
		Style.SetUncheckedHoveredImage(UncheckedHovered);
		Style.SetUncheckedPressedImage(UncheckedHovered);
		Style.SetCheckedImage(Checked);
		Style.SetCheckedHoveredImage(CheckedHovered);
		Style.SetCheckedPressedImage(CheckedHovered);
		Style.SetUndeterminedImage(Unchecked);
		Style.SetUndeterminedHoveredImage(UncheckedHovered);
		Style.SetUndeterminedPressedImage(UncheckedHovered);
		Style.SetPadding(FMargin(0.f));

		CheckBox.SetWidgetStyle(Style);
	}
}
