#include "UI/ConfiguratorButton.h"

UConfiguratorButton::UConfiguratorButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitIsFocusable(false);
}

UConfiguratorCheckBox::UConfiguratorCheckBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitIsFocusable(false);
}
