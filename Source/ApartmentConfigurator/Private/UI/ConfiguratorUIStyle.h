#pragma once

#include "CoreMinimal.h"

class UBorder;
class UButton;
class UCheckBox;
class UTextBlock;
class UWidget;
class UWidgetTree;

/**
 * Дизайн-система интерфейса, собираемого в C++: палитра и «кирпичики»
 * (скруглённые панели, кнопки с состояниями, подписи). Всё рисуется процедурными
 * кистями Slate (FSlateRoundedBoxBrush) — без текстур и WBP-ассетов.
 *
 * Панель, карточка и кнопки берут цвета и радиусы только отсюда, поэтому интерфейс
 * выглядит единообразно, а перекрасить его можно в одном месте.
 */
namespace ConfiguratorUI
{
	/** Цвет из sRGB-кода вида "#1F2630" (так удобнее сверять с макетом). */
	FLinearColor Hex(const TCHAR* HexCode, float Alpha = 1.f);

	// ---------------------------------------------------------------- Палитра
	extern const FLinearColor PanelColor;
	extern const FLinearColor PanelOutlineColor;
	extern const FLinearColor SurfaceColor;			// нейтральные кнопки
	extern const FLinearColor DividerColor;
	extern const FLinearColor TextColor;
	extern const FLinearColor MutedTextColor;
	extern const FLinearColor AccentColor;			// выбранный элемент
	extern const FLinearColor AccentTextColor;		// текст на акцентном фоне
	extern const FLinearColor PrimaryColor;			// главное действие («Забронировать»)
	extern const FLinearColor AvailableColor;		// яркие цвета статусов: текст, точки, «таблетки»
	extern const FLinearColor SoldColor;
	extern const FLinearColor UnknownColor;
	extern const FLinearColor AvailableChipColor;	// приглушённые фоны плиток квартир
	extern const FLinearColor SoldChipColor;
	extern const FLinearColor UnknownChipColor;

	constexpr float PanelRadius = 14.f;
	constexpr float ButtonRadius = 8.f;

	// ---------------------------------------------------------------- Кирпичики
	UTextBlock* MakeText(UWidgetTree& Tree, const FText& Text, int32 FontSize, const FLinearColor& Color, bool bBold = false);

	/** Мелкий заголовок секции: приглушённый, с разрядкой (текст передаётся уже в верхнем регистре). */
	UTextBlock* MakeCaption(UWidgetTree& Tree, const FText& Text);

	/** Скруглённая подложка произвольного цвета (панель, «таблетка», точка легенды, разделитель). */
	UBorder* MakeRoundedBox(UWidgetTree& Tree, const FLinearColor& Color, float Radius, const FMargin& Padding);

	/** Основная панель: тёмная полупрозрачная, со скруглением и тонкой светлой обводкой. */
	UBorder* MakePanel(UWidgetTree& Tree, const FMargin& Padding);

	/** Горизонтальный разделитель толщиной 1 px. */
	UWidget* MakeDivider(UWidgetTree& Tree);

	/**
	 * Задаёт кнопке скруглённый стиль. Цвета наведения, нажатия и отключённого состояния
	 * выводятся из базового, поэтому кнопка любого цвета реагирует на курсор одинаково.
	 * OutlineWidth > 0 — обводка (используется для выделения).
	 */
	void StyleButton(UButton& Button, const FLinearColor& Color, float Radius = ButtonRadius,
		const FLinearColor& OutlineColor = FLinearColor::Transparent, float OutlineWidth = 0.f);

	/** Кнопка без содержимого (не забирает фокус клавиатуры). */
	UButton* MakeButton(UWidgetTree& Tree, const FLinearColor& Color, float Radius = ButtonRadius);

	/** Кнопка с текстовой подписью. OutLabel — подпись, если её нужно менять позже. */
	UButton* MakeTextButton(UWidgetTree& Tree, const FText& Text, const FLinearColor& Color,
		UTextBlock** OutLabel = nullptr, int32 FontSize = 12, const FMargin& Padding = FMargin(12.f, 7.f));

	/** Чекбокс в стиле интерфейса: скруглённый квадрат, в отмеченном состоянии — акцентная заливка. */
	void StyleCheckBox(UCheckBox& CheckBox);
}
