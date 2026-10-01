#include "Core/ConfiguratorStateSubsystem.h"

#include "ApartmentConfigurator.h"
#include "ApartmentDataSubsystem.h"
#include "Core/ConfiguratorSceneProvider.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UConfiguratorStateSubsystem::Deinitialize()
{
	OnViewChanged.Clear();
	OnFilterChanged.Clear();
	History.Reset();
	SceneProviderObject.Reset();

	Super::Deinitialize();
}

bool UConfiguratorStateSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// ---------------------------------------------------------------- Навигация

void UConfiguratorStateSubsystem::ShowGenplan()
{
	NavigateTo(FConfiguratorViewState::MakeGenplan());
}

void UConfiguratorStateSubsystem::ShowFloor(int32 FloorIndex)
{
	const FConfiguratorViewState NewView = FConfiguratorViewState::MakeFloor(FloorIndex);
	if (!IsViewValid(NewView))
	{
		UE_LOG(LogConfigurator, Warning, TEXT("ShowFloor: invalid floor index %d"), FloorIndex);
		return;
	}
	NavigateTo(NewView);
}

bool UConfiguratorStateSubsystem::ShowApartment(const FString& ApartmentId)
{
	const UApartmentDataSubsystem* Data = GetDataSubsystem();
	int32 FloorIndex = INDEX_NONE;
	const FApartmentData* Apartment = Data ? Data->FindApartment(ApartmentId, &FloorIndex) : nullptr;

	if (!Apartment)
	{
		UE_LOG(LogConfigurator, Warning, TEXT("ShowApartment: apartment '%s' not found"), *ApartmentId);
		return false;
	}
	if (!IsApartmentSelectable(*Apartment))
	{
		UE_LOG(LogConfigurator, Verbose, TEXT("ShowApartment: apartment '%s' is hidden by the filter"), *ApartmentId);
		return false;
	}

	NavigateTo(FConfiguratorViewState::MakeApartment(FloorIndex, Apartment->Id));
	return true;
}

bool UConfiguratorStateSubsystem::GoBack()
{
	// Снимки в истории могли устареть (например, квартира скрыта фильтром) — такие пропускаем.
	while (History.Num() > 0)
	{
		const FConfiguratorViewState Previous = History.Pop(EAllowShrinking::No);
		if (IsViewValid(Previous) && Previous != CurrentView)
		{
			SetCurrentView(Previous, EConfiguratorViewChangeReason::Back);
			return true;
		}
	}
	return false;
}

void UConfiguratorStateSubsystem::NavigateTo(const FConfiguratorViewState& NewView)
{
	if (NewView == CurrentView)
	{
		return;
	}

	History.Push(CurrentView);
	if (History.Num() > MaxHistoryDepth)
	{
		History.RemoveAt(0, 1, EAllowShrinking::No);
	}

	SetCurrentView(NewView, EConfiguratorViewChangeReason::Navigate);
}

void UConfiguratorStateSubsystem::SetCurrentView(const FConfiguratorViewState& NewView, EConfiguratorViewChangeReason Reason)
{
	CurrentView = NewView;
	UE_LOG(LogConfigurator, Log, TEXT("View changed (%s): mode %s, floor %d, apartment '%s'"),
		*UEnum::GetValueAsString(Reason), *UEnum::GetValueAsString(CurrentView.Mode), CurrentView.FloorIndex, *CurrentView.ApartmentId);
	OnViewChanged.Broadcast(CurrentView, Reason);
}

bool UConfiguratorStateSubsystem::IsViewValid(const FConfiguratorViewState& View) const
{
	if (View.Mode == EConfiguratorViewMode::Genplan)
	{
		return true;
	}

	const UApartmentDataSubsystem* Data = GetDataSubsystem();
	if (!Data || !Data->HasData() || !Data->GetBuildingData().Floors.IsValidIndex(View.FloorIndex))
	{
		return false;
	}

	if (View.Mode == EConfiguratorViewMode::Floor)
	{
		return true;
	}

	int32 FloorIndex = INDEX_NONE;
	const FApartmentData* Apartment = Data->FindApartment(View.ApartmentId, &FloorIndex);
	return Apartment && FloorIndex == View.FloorIndex && IsApartmentSelectable(*Apartment);
}

// ---------------------------------------------------------------- Фильтр

void UConfiguratorStateSubsystem::SetHideSold(bool bInHideSold)
{
	if (bHideSold == bInHideSold)
	{
		return;
	}

	bHideSold = bInHideSold;
	OnFilterChanged.Broadcast(bHideSold);

	// Если в фокусе была проданная квартира, а её только что скрыли — возвращаемся к её этажу.
	// Это замена текущего состояния, а не новый шаг истории.
	if (CurrentView.Mode == EConfiguratorViewMode::Apartment && !IsViewValid(CurrentView))
	{
		SetCurrentView(FConfiguratorViewState::MakeFloor(CurrentView.FloorIndex), EConfiguratorViewChangeReason::Navigate);
	}
}

bool UConfiguratorStateSubsystem::IsApartmentSelectable(const FApartmentData& Apartment) const
{
	return !(bHideSold && Apartment.IsSold());
}

// ---------------------------------------------------------------- Сцена

void UConfiguratorStateSubsystem::RegisterSceneProvider(UObject* ProviderObject)
{
	if (ProviderObject && Cast<IConfiguratorSceneProvider>(ProviderObject))
	{
		SceneProviderObject = ProviderObject;
	}
	else
	{
		UE_LOG(LogConfigurator, Error, TEXT("RegisterSceneProvider: object does not implement IConfiguratorSceneProvider"));
	}
}

void UConfiguratorStateSubsystem::UnregisterSceneProvider(UObject* ProviderObject)
{
	if (SceneProviderObject.Get() == ProviderObject)
	{
		SceneProviderObject.Reset();
	}
}

IConfiguratorSceneProvider* UConfiguratorStateSubsystem::GetSceneProvider() const
{
	return Cast<IConfiguratorSceneProvider>(SceneProviderObject.Get());
}

void UConfiguratorStateSubsystem::NotifySceneRebuilt()
{
	// Индексы этажей и ID квартир в истории относятся к старым данным — историю очищаем.
	History.Reset();
	SetCurrentView(FConfiguratorViewState::MakeGenplan(), EConfiguratorViewChangeReason::Reset);
}

UApartmentDataSubsystem* UConfiguratorStateSubsystem::GetDataSubsystem() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UApartmentDataSubsystem>() : nullptr;
}
