#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "ConfiguratorButton.generated.h"

/**
 * Кнопка интерфейса конфигуратора, не забирающая фокус клавиатуры.
 *
 * Иначе после клика Space/Enter повторно «нажимали» бы последнюю кнопку.
 * В UE 5.7 фокусируемость задаётся только защищённым UButton::InitIsFocusable
 * в конструкторе, поэтому нужен наследник.
 */
UCLASS()
class APARTMENTCONFIGURATOR_API UConfiguratorButton : public UButton
{
	GENERATED_BODY()

public:
	UConfiguratorButton(const FObjectInitializer& ObjectInitializer);
};

/** Чекбокс, не забирающий фокус клавиатуры (по той же причине, что и UConfiguratorButton). */
UCLASS()
class APARTMENTCONFIGURATOR_API UConfiguratorCheckBox : public UCheckBox
{
	GENERATED_BODY()

public:
	UConfiguratorCheckBox(const FObjectInitializer& ObjectInitializer);
};
