#include "ApartmentDataSubsystem.h"

#include "Async/Async.h"
#include "ConfiguratorDataSettings.h"
#include "ConfiguratorJsonParser.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	/**
	 * Консольная команда для демонстрации и отладки:
	 *   Configurator.Load                            — перечитать файл из настроек проекта;
	 *   Configurator.Load Data/Building_Broken.json  — загрузить другой файл (путь от папки Content).
	 */
	FAutoConsoleCommandWithWorldAndArgs GConfiguratorLoadCommand(
		TEXT("Configurator.Load"),
		TEXT("Reload configurator JSON. Optional argument: file path relative to the Content directory."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			UApartmentDataSubsystem* Data = GameInstance ? GameInstance->GetSubsystem<UApartmentDataSubsystem>() : nullptr;
			if (!Data)
			{
				UE_LOG(LogConfiguratorData, Warning, TEXT("Configurator.Load: no game instance (run the command in PIE or in the game)"));
				return;
			}

			if (Args.Num() > 0)
			{
				Data->LoadFromFile(FString::Join(Args, TEXT(" ")));
			}
			else
			{
				Data->RequestLoad();
			}
		}));

	/** Защита от случайно подсунутого огромного файла: конфигурация здания весит килобайты. */
	constexpr int64 MaxDataFileSizeBytes = 16 * 1024 * 1024;

	FString ResolvePath(const FString& FilePath)
	{
		const FString Combined = FPaths::IsRelative(FilePath)
			? FPaths::Combine(FPaths::ProjectContentDir(), FilePath)
			: FilePath;
		return FPaths::ConvertRelativePathToFull(Combined);
	}
}

void UApartmentDataSubsystem::Deinitialize()
{
	// Инвалидируем незавершённый запрос: даже если фоновая задача успеет закончиться,
	// её результат не будет применён (а слабый указатель не даст обратиться к удалённому объекту).
	++LatestRequestId;
	OnDataLoaded.Clear();
	OnLoadFailed.Clear();

	Super::Deinitialize();
}

void UApartmentDataSubsystem::RequestLoad()
{
	const UConfiguratorDataSettings* Settings = GetDefault<UConfiguratorDataSettings>();
	LoadFromFile(Settings->DataFilePath);
}

void UApartmentDataSubsystem::LoadFromFile(const FString& FilePath)
{
	const FString FullPath = ResolvePath(FilePath);
	const uint32 RequestId = ++LatestRequestId;
	const float SimulatedDelay = GetDefault<UConfiguratorDataSettings>()->SimulatedLoadDelaySeconds;

	LoadState = EConfiguratorLoadState::Loading;
	UE_LOG(LogConfiguratorData, Log, TEXT("Loading configurator data from '%s' (request %u)"), *FullPath, RequestId);

	TWeakObjectPtr<UApartmentDataSubsystem> WeakThis(this);

	// Фоновая часть: только файловый ввод-вывод и парсинг в обычные структуры, никаких UObject.
	Async(EAsyncExecution::ThreadPool, [WeakThis, FullPath, RequestId, SimulatedDelay]()
	{
		if (SimulatedDelay > 0.f)
		{
			FPlatformProcess::Sleep(SimulatedDelay);
		}

		FConfiguratorParseResult Result;
		const int64 FileSize = IFileManager::Get().FileSize(*FullPath);

		if (FileSize < 0)
		{
			Result.Error = FString::Printf(TEXT("File not found: %s"), *FullPath);
		}
		else if (FileSize > MaxDataFileSizeBytes)
		{
			Result.Error = FString::Printf(TEXT("File is too large (%lld bytes): %s"), FileSize, *FullPath);
		}
		else
		{
			FString JsonText;
			if (FFileHelper::LoadFileToString(JsonText, *FullPath))
			{
				Result = FConfiguratorJsonParser::ParseString(JsonText);
			}
			else
			{
				Result.Error = FString::Printf(TEXT("Cannot read file: %s"), *FullPath);
			}
		}

		// Возврат на игровой поток: только там можно менять состояние подсистемы и вызывать делегаты.
		AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId, Result = MoveTemp(Result)]() mutable
		{
			if (UApartmentDataSubsystem* Subsystem = WeakThis.Get())
			{
				Subsystem->ApplyParseResult(RequestId, MoveTemp(Result));
			}
		});
	});
}

void UApartmentDataSubsystem::ApplyParseResult(uint32 RequestId, FConfiguratorParseResult&& Result)
{
	check(IsInGameThread());

	if (RequestId != LatestRequestId)
	{
		UE_LOG(LogConfiguratorData, Verbose, TEXT("Discarding stale load result (request %u, latest %u)"), RequestId, LatestRequestId);
		return;
	}

	if (!Result.bSuccess)
	{
		LastError = Result.Error;
		LoadState = EConfiguratorLoadState::Failed;
		UE_LOG(LogConfiguratorData, Error, TEXT("Failed to load configurator data: %s%s"), *LastError,
			bHasData ? TEXT(" (previous data is kept)") : TEXT(""));
		OnLoadFailed.Broadcast(LastError);
		return;
	}

	BuildingData = MoveTemp(Result.Data);
	LastWarnings = MoveTemp(Result.Warnings);
	LastError.Reset();
	bHasData = true;
	LoadState = EConfiguratorLoadState::Loaded;

	RebuildApartmentIndex();

	for (const FString& Warning : LastWarnings)
	{
		UE_LOG(LogConfiguratorData, Warning, TEXT("Data: %s"), *Warning);
	}
	UE_LOG(LogConfiguratorData, Log, TEXT("Configurator data loaded: %d floors, %d apartments, %d warnings"),
		BuildingData.Floors.Num(), BuildingData.GetApartmentCount(), LastWarnings.Num());

	OnDataLoaded.Broadcast();
}

void UApartmentDataSubsystem::RebuildApartmentIndex()
{
	ApartmentIndex.Reset();
	for (int32 FloorIndex = 0; FloorIndex < BuildingData.Floors.Num(); ++FloorIndex)
	{
		const TArray<FApartmentData>& Apartments = BuildingData.Floors[FloorIndex].Apartments;
		for (int32 ApartmentIdx = 0; ApartmentIdx < Apartments.Num(); ++ApartmentIdx)
		{
			// Парсер уже гарантирует уникальность ID, повторная проверка — страховка.
			if (!ApartmentIndex.Contains(Apartments[ApartmentIdx].Id))
			{
				ApartmentIndex.Add(Apartments[ApartmentIdx].Id, FIntPoint(FloorIndex, ApartmentIdx));
			}
		}
	}
}

const FApartmentData* UApartmentDataSubsystem::FindApartment(const FString& ApartmentId, int32* OutFloorIndex) const
{
	const FIntPoint* Location = ApartmentIndex.Find(ApartmentId);
	if (!Location || !BuildingData.Floors.IsValidIndex(Location->X)
		|| !BuildingData.Floors[Location->X].Apartments.IsValidIndex(Location->Y))
	{
		return nullptr;
	}

	if (OutFloorIndex)
	{
		*OutFloorIndex = Location->X;
	}
	return &BuildingData.Floors[Location->X].Apartments[Location->Y];
}

bool UApartmentDataSubsystem::FindApartmentCopy(const FString& ApartmentId, FApartmentData& OutApartment, int32& OutFloorIndex) const
{
	OutFloorIndex = INDEX_NONE;
	if (const FApartmentData* Apartment = FindApartment(ApartmentId, &OutFloorIndex))
	{
		OutApartment = *Apartment;
		return true;
	}
	return false;
}
