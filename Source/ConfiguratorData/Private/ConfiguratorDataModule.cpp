#include "ConfiguratorDataTypes.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogConfiguratorData);

#define LOCTEXT_NAMESPACE "ConfiguratorData"

FText GetApartmentStatusText(EApartmentStatus Status)
{
	switch (Status)
	{
	case EApartmentStatus::Available:	return LOCTEXT("StatusAvailable", "Свободно");
	case EApartmentStatus::Sold:		return LOCTEXT("StatusSold", "Продано");
	default:							return LOCTEXT("StatusUnknown", "Неизвестно");
	}
}

#undef LOCTEXT_NAMESPACE

// Модулю не нужна собственная логика запуска/выгрузки — достаточно стандартной реализации.
IMPLEMENT_MODULE(FDefaultModuleImpl, ConfiguratorData);
