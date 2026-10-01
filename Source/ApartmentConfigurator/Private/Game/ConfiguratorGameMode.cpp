#include "Game/ConfiguratorGameMode.h"

#include "ApartmentConfigurator.h"
#include "ApartmentDataSubsystem.h"
#include "Camera/ConfiguratorCameraPawn.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Player/ConfiguratorPlayerController.h"
#include "Scene/ConfiguratorBuildingActor.h"
#include "Scene/ConfiguratorSiteActor.h"

AConfiguratorGameMode::AConfiguratorGameMode()
{
	DefaultPawnClass = AConfiguratorCameraPawn::StaticClass();
	PlayerControllerClass = AConfiguratorPlayerController::StaticClass();
	BuildingClass = AConfiguratorBuildingActor::StaticClass();
	SiteClass = AConfiguratorSiteActor::StaticClass();
}

void AConfiguratorGameMode::StartPlay()
{
	Super::StartPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (TActorIterator<ADirectionalLight> Sun(World); Sun)
	{
		if (SunElevation > 0.f)
		{
			// Меняем только высоту солнца, азимут остаётся как на уровне. Атмосфера и небесный свет
			// (real-time capture) подстраиваются под новое направление сами.
			FRotator SunRotation = Sun->GetActorRotation();
			SunRotation.Pitch = -SunElevation;
			SunRotation.Roll = 0.f;
			Sun->SetActorRotation(SunRotation);
		}

		if (bAlignFacadeToSun)
		{
			// Фасад смотрит в -Y локальных координат здания. Чтобы он смотрел навстречу свету,
			// yaw здания = yaw направления света - 90 градусов.
			const FVector LightDirection = Sun->GetActorForwardVector();
			const float LightYaw = FMath::RadiansToDegrees(FMath::Atan2(LightDirection.Y, LightDirection.X));
			DefaultBuildingTransform.SetRotation(FRotator(0.f, LightYaw - 90.f + SunFacadeAngleOffset, 0.f).Quaternion());
		}
	}

	// Здание и благоустройство можно заранее поставить на уровень (и настроить в Details),
	// иначе создаём экземпляры по умолчанию.
	SpawnIfMissing(BuildingClass);
	SpawnIfMissing(SiteClass);

	// Загрузка асинхронная: уровень, камера и UI уже работают, здание появится, когда данные будут готовы.
	if (UApartmentDataSubsystem* Data = GetGameInstance()->GetSubsystem<UApartmentDataSubsystem>())
	{
		if (!Data->HasData() && Data->GetLoadState() != EConfiguratorLoadState::Loading)
		{
			Data->RequestLoad();
		}
	}
}

void AConfiguratorGameMode::SpawnIfMissing(UClass* Class)
{
	UWorld* World = GetWorld();
	if (!World || !Class)
	{
		return;
	}

	if (TActorIterator<AActor>(World, Class))
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<AActor>(Class, DefaultBuildingTransform, SpawnParams);
}
