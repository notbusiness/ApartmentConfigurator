#include "Scene/ConfiguratorSiteActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace SiteLooks
{
	const FSurfaceLook Grass(FLinearColor(0.085f, 0.18f, 0.045f), 0.95f, 0.f, ESurfacePattern::Grass);
	const FSurfaceLook Paving(FLinearColor(0.26f, 0.25f, 0.235f), 0.8f, 0.f, ESurfacePattern::Paving);
	const FSurfaceLook Curb(FLinearColor(0.42f, 0.42f, 0.40f), 0.8f, 0.f, ESurfacePattern::Concrete);
	const FSurfaceLook Asphalt(FLinearColor(0.03f, 0.03f, 0.035f), 0.75f, 0.f, ESurfacePattern::Asphalt);
	const FSurfaceLook Marking(FLinearColor(0.75f, 0.75f, 0.72f), 0.6f);
	const FSurfaceLook Trunk(FLinearColor(0.10f, 0.07f, 0.045f), 0.9f, 0.f, ESurfacePattern::Concrete);
	const FSurfaceLook Foliage(FLinearColor(0.05f, 0.13f, 0.028f), 0.85f, 0.f, ESurfacePattern::Foliage);
	const FSurfaceLook FoliageAlt(FLinearColor(0.085f, 0.17f, 0.035f), 0.85f, 0.f, ESurfacePattern::Foliage);
	const FSurfaceLook Conifer(FLinearColor(0.025f, 0.075f, 0.03f), 0.9f, 0.f, ESurfacePattern::Foliage);
	const FSurfaceLook Metal(FLinearColor(0.05f, 0.05f, 0.06f), 0.4f, 0.8f);
	const FSurfaceLook Wood(FLinearColor(0.20f, 0.12f, 0.06f), 0.7f);
	const FSurfaceLook DistantLight(FLinearColor(0.46f, 0.46f, 0.45f), 0.85f, 0.f, ESurfacePattern::DistantBuilding);
	const FSurfaceLook DistantWarm(FLinearColor(0.46f, 0.40f, 0.33f), 0.85f, 0.f, ESurfacePattern::DistantBuilding);
	const FSurfaceLook DistantBrick(FLinearColor(0.33f, 0.19f, 0.13f), 0.85f, 0.f, ESurfacePattern::DistantBuilding);
}

namespace
{
	/** Базовые фигуры движка: 100 см по каждой оси, пивот в центре (у плоскости — нормаль вверх). */
	constexpr double SiteShapeSize = 100.0;

	/** Детерминированное псевдослучайное число 0..1: сцена одинакова при каждом запуске. */
	float SiteRandom(int32 Seed, int32 Salt)
	{
		return FMath::Frac(FMath::Abs(FMath::Sin(Seed * 12.9898f + Salt * 78.233f) * 43758.5453f));
	}
}

AConfiguratorSiteActor::AConfiguratorSiteActor()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

	// Пост-процесс на всю сцену: чуть ниже экспозиция (светлые фасады на солнце не «выгорают»),
	// немного насыщенности и лёгкая виньетка — картинка выглядит как архитектурная визуализация.
	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(RootComponent);
	PostProcess->bUnbound = true;
	PostProcess->Priority = 10.f;
	PostProcess->Settings.bOverride_AutoExposureBias = true;
	PostProcess->Settings.AutoExposureBias = -0.45f;
	PostProcess->Settings.bOverride_ColorSaturation = true;
	PostProcess->Settings.ColorSaturation = FVector4(1.12f, 1.12f, 1.12f, 1.f);
	PostProcess->Settings.bOverride_ColorContrast = true;
	PostProcess->Settings.ColorContrast = FVector4(1.06f, 1.06f, 1.06f, 1.f);
	PostProcess->Settings.bOverride_VignetteIntensity = true;
	PostProcess->Settings.VignetteIntensity = 0.3f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SurfaceFinder(TEXT("/Game/Configurator/Materials/M_ConfiguratorSurface.M_ConfiguratorSurface"));

	CubeMesh = CubeFinder.Object;
	PlaneMesh = PlaneFinder.Object;
	CylinderMesh = CylinderFinder.Object;
	SphereMesh = SphereFinder.Object;
	ConeMesh = ConeFinder.Object;
	SurfaceMaterial = SurfaceFinder.Object;
}

void AConfiguratorSiteActor::BeginPlay()
{
	Super::BeginPlay();

	BuildGroundAndRoad();
	BuildPlanting();
	BuildStreetFurniture();
	if (bBuildDistantBuildings)
	{
		BuildDistantBuildings();
	}
}

// ---------------------------------------------------------------- Инстансы

void AConfiguratorSiteActor::AddInstance(UStaticMesh* Mesh, const FSurfaceLook& Look, const FName& Key, const FTransform& Transform)
{
	if (!Mesh)
	{
		return;
	}

	TObjectPtr<UInstancedStaticMeshComponent>& Component = Parts.FindOrAdd(Key);
	if (!Component)
	{
		Component = NewObject<UInstancedStaticMeshComponent>(this, Key);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		if (SurfaceMaterial)
		{
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(SurfaceMaterial, this);
			Look.ApplyTo(*Material, GetActorTransform());
			Component->SetMaterial(0, Material);
		}
		Component->SetupAttachment(RootComponent);
		Component->RegisterComponent();
		AddInstanceComponent(Component);
	}

	Component->AddInstance(Transform);
}

void AConfiguratorSiteActor::AddBox(const FSurfaceLook& Look, const FName& Key, const FVector& Min, const FVector& Max)
{
	AddInstance(CubeMesh, Look, Key, FTransform(FQuat::Identity, (Min + Max) * 0.5, (Max - Min).GetAbs() / SiteShapeSize));
}

// ---------------------------------------------------------------- Озеленение и малые формы

void AConfiguratorSiteActor::AddTree(const FVector& Base, float Scale, int32 Seed)
{
	const float TrunkHeight = (260.f + 80.f * SiteRandom(Seed, 1)) * Scale;
	AddInstance(CylinderMesh, SiteLooks::Trunk, TEXT("Site_Trunk"),
		FTransform(FQuat::Identity, Base + FVector(0.f, 0.f, TrunkHeight * 0.5f), FVector(0.26f * Scale, 0.26f * Scale, TrunkHeight / SiteShapeSize)));

	const bool bAlt = SiteRandom(Seed, 2) > 0.5f;
	const FSurfaceLook& Look = bAlt ? SiteLooks::FoliageAlt : SiteLooks::Foliage;
	const FName Key = bAlt ? FName(TEXT("Site_FoliageAlt")) : FName(TEXT("Site_Foliage"));

	// Крона: крупный центральный объём и несколько объёмов поменьше вокруг — неровный «живой» силуэт.
	const FVector CrownCenter = Base + FVector(0.f, 0.f, TrunkHeight + 130.f * Scale);
	AddInstance(SphereMesh, Look, Key, FTransform(FQuat::Identity, CrownCenter, FVector(3.7f, 3.7f, 3.1f) * Scale));

	const int32 Lobes = 6;
	for (int32 Lobe = 0; Lobe < Lobes; ++Lobe)
	{
		const float Angle = (Lobe + SiteRandom(Seed, 10 + Lobe) * 0.7f) * UE_TWO_PI / Lobes;
		const float Distance = (105.f + 60.f * SiteRandom(Seed, 20 + Lobe)) * Scale;
		const float Height = (-50.f + 170.f * SiteRandom(Seed, 30 + Lobe)) * Scale;
		const float Size = (1.9f + 0.9f * SiteRandom(Seed, 40 + Lobe)) * Scale;
		AddInstance(SphereMesh, Look, Key, FTransform(FQuat::Identity,
			CrownCenter + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, Height),
			FVector(Size, Size, Size * 0.85f)));
	}
}

void AConfiguratorSiteActor::AddConifer(const FVector& Base, float Scale, int32 Seed)
{
	const float TrunkHeight = 130.f * Scale;
	AddInstance(CylinderMesh, SiteLooks::Trunk, TEXT("Site_Trunk"),
		FTransform(FQuat::Identity, Base + FVector(0.f, 0.f, TrunkHeight * 0.5f), FVector(0.2f * Scale, 0.2f * Scale, TrunkHeight / SiteShapeSize)));

	// Ярусы: каждый следующий конус уже и выше предыдущего.
	const int32 Tiers = 4;
	for (int32 Tier = 0; Tier < Tiers; ++Tier)
	{
		const float Diameter = (360.f - Tier * 72.f) * Scale * (0.92f + 0.16f * SiteRandom(Seed, Tier));
		const float Height = 230.f * Scale;
		const float BaseZ = (90.f + Tier * 115.f) * Scale;
		AddInstance(ConeMesh, SiteLooks::Conifer, TEXT("Site_Conifer"),
			FTransform(FQuat::Identity, Base + FVector(0.f, 0.f, BaseZ + Height * 0.5f),
				FVector(Diameter / SiteShapeSize, Diameter / SiteShapeSize, Height / SiteShapeSize)));
	}
}

void AConfiguratorSiteActor::AddBush(const FVector& Base, float Scale, int32 Seed)
{
	for (int32 Part = 0; Part < 3; ++Part)
	{
		const float Size = (0.9f + 0.5f * SiteRandom(Seed, Part)) * Scale;
		const FVector Offset((SiteRandom(Seed, 5 + Part) - 0.5f) * 90.f * Scale, (SiteRandom(Seed, 9 + Part) - 0.5f) * 60.f * Scale, Size * 32.f);
		AddInstance(SphereMesh, SiteLooks::FoliageAlt, TEXT("Site_FoliageAlt"),
			FTransform(FQuat::Identity, Base + Offset, FVector(Size, Size, Size * 0.75f)));
	}
}

void AConfiguratorSiteActor::AddLamp(const FVector& Base)
{
	const float Height = 520.f;
	AddInstance(CylinderMesh, SiteLooks::Metal, TEXT("Site_LampPole"),
		FTransform(FQuat::Identity, Base + FVector(0.f, 0.f, Height * 0.5f), FVector(0.12f, 0.12f, Height / SiteShapeSize)));
	AddBox(SiteLooks::Metal, TEXT("Site_Metal"), Base + FVector(-15.f, -70.f, Height - 12.f), Base + FVector(15.f, 10.f, Height + 4.f));
}

void AConfiguratorSiteActor::AddBench(const FVector& Center, bool bAlongX)
{
	const FVector HalfSeat = bAlongX ? FVector(90.f, 22.f, 3.f) : FVector(22.f, 90.f, 3.f);
	const FVector Seat = Center + FVector(0.f, 0.f, 45.f);
	AddBox(SiteLooks::Wood, TEXT("Site_Wood"), Seat - HalfSeat, Seat + HalfSeat);

	const FVector LegOffset = bAlongX ? FVector(70.f, 0.f, 0.f) : FVector(0.f, 70.f, 0.f);
	const FVector HalfLeg = bAlongX ? FVector(4.f, 20.f, 21.f) : FVector(20.f, 4.f, 21.f);
	for (const float Side : { -1.f, 1.f })
	{
		const FVector Leg = Center + LegOffset * Side + FVector(0.f, 0.f, 21.f);
		AddBox(SiteLooks::Metal, TEXT("Site_Metal"), Leg - HalfLeg, Leg + HalfLeg);
	}
}

void AConfiguratorSiteActor::AddLawnBed(const FVector2D& Min, const FVector2D& Max, int32 Seed)
{
	// Бордюр и приподнятый газон внутри него.
	AddBox(SiteLooks::Curb, TEXT("Site_Curb"), FVector(Min.X - 8.f, Min.Y - 8.f, 0.f), FVector(Max.X + 8.f, Max.Y + 8.f, 15.f));
	AddBox(SiteLooks::Grass, TEXT("Site_GrassBed"), FVector(Min.X, Min.Y, 4.f), FVector(Max.X, Max.Y, 19.f));

	const float Width = Max.X - Min.X;
	const int32 Bushes = FMath::Max(1, FMath::RoundToInt32(Width / 280.f));
	for (int32 Index = 0; Index < Bushes; ++Index)
	{
		const float X = Min.X + Width * (Index + 0.5f) / Bushes + (SiteRandom(Seed, Index) - 0.5f) * 80.f;
		const float Y = FMath::Lerp(Min.Y, Max.Y, 0.3f + 0.4f * SiteRandom(Seed, 50 + Index));
		AddBush(FVector(X, Y, 19.f), 0.9f + 0.4f * SiteRandom(Seed, 90 + Index), Seed * 31 + Index);
	}
}

void AConfiguratorSiteActor::AddDistantBuilding(const FVector2D& Center, const FVector2D& Size, int32 Floors, int32 Seed)
{
	const float Height = Floors * 300.f;
	const int32 Variant = Seed % 3;
	const FSurfaceLook& Look = Variant == 0 ? SiteLooks::DistantLight : (Variant == 1 ? SiteLooks::DistantWarm : SiteLooks::DistantBrick);
	const FName Key = Variant == 0 ? FName(TEXT("Site_DistantLight")) : (Variant == 1 ? FName(TEXT("Site_DistantWarm")) : FName(TEXT("Site_DistantBrick")));

	AddBox(Look, Key, FVector(Center.X - Size.X * 0.5f, Center.Y - Size.Y * 0.5f, 0.f), FVector(Center.X + Size.X * 0.5f, Center.Y + Size.Y * 0.5f, Height));
}

// ---------------------------------------------------------------- Сборка участка

void AConfiguratorSiteActor::BuildGroundAndRoad()
{
	// Газон до горизонта: заодно закрывает шахматный пол шаблонного уровня.
	AddInstance(PlaneMesh, SiteLooks::Grass, TEXT("Site_Grass"),
		FTransform(FQuat::Identity, FVector(0.f, 0.f, 1.f), FVector(4000.f, 4000.f, 1.f)));

	// Мощёная площадка вокруг здания.
	AddBox(SiteLooks::Paving, TEXT("Site_Paving"),
		FVector(-PlazaHalfSize.X, -PlazaHalfSize.Y, 0.f), FVector(PlazaHalfSize.X, PlazaHalfSize.Y, 4.f));

	// Тротуар с бордюрами между площадкой и дорогой.
	const float RoadNearY = -RoadOffset;
	const float RoadFarY = RoadNearY - RoadWidth;
	const float StreetHalfLength = 30000.f;
	AddBox(SiteLooks::Paving, TEXT("Site_Paving"), FVector(-StreetHalfLength, RoadNearY + 50.f, 0.f), FVector(StreetHalfLength, -PlazaHalfSize.Y, 12.f));
	AddBox(SiteLooks::Curb, TEXT("Site_Curb"), FVector(-StreetHalfLength, RoadNearY, 0.f), FVector(StreetHalfLength, RoadNearY + 50.f, 16.f));
	AddBox(SiteLooks::Curb, TEXT("Site_Curb"), FVector(-StreetHalfLength, RoadFarY - 50.f, 0.f), FVector(StreetHalfLength, RoadFarY, 16.f));

	// Дорога, прерывистая осевая и пешеходный переход напротив здания.
	AddBox(SiteLooks::Asphalt, TEXT("Site_Asphalt"), FVector(-StreetHalfLength, RoadFarY, 0.f), FVector(StreetHalfLength, RoadNearY, 5.f));

	const float CrossingHalfWidth = 200.f;
	const float MarkingY = (RoadNearY + RoadFarY) * 0.5f;
	for (float X = -StreetHalfLength; X < StreetHalfLength; X += 600.f)
	{
		if (FMath::Abs(X + 150.f) > CrossingHalfWidth + 350.f)
		{
			AddBox(SiteLooks::Marking, TEXT("Site_Marking"), FVector(X, MarkingY - 6.f, 5.f), FVector(X + 300.f, MarkingY + 6.f, 6.f));
		}
	}
	for (float Y = RoadFarY + 30.f; Y < RoadNearY - 40.f; Y += 80.f)
	{
		AddBox(SiteLooks::Marking, TEXT("Site_Marking"), FVector(-CrossingHalfWidth, Y, 5.f), FVector(CrossingHalfWidth, Y + 40.f, 6.f));
	}
}

void AConfiguratorSiteActor::BuildPlanting()
{
	const float RoadNearY = -RoadOffset;
	int32 Seed = 1;

	// Аллея вдоль тротуара (с разрывом у перехода).
	for (float X = -6600.f; X <= 6600.f; X += 1100.f)
	{
		if (FMath::Abs(X) > 500.f)
		{
			AddTree(FVector(X, RoadNearY + 260.f, 12.f), 0.95f + 0.25f * SiteRandom(Seed, 3), Seed);
		}
		++Seed;
	}

	// Двор за домом: лиственные деревья и группа елей.
	for (float X = -4500.f; X <= 4500.f; X += 1500.f)
	{
		AddTree(FVector(X + 200.f * (SiteRandom(Seed, 4) - 0.5f), PlazaHalfSize.Y + 650.f, 1.f), 1.0f + 0.3f * SiteRandom(Seed, 3), Seed);
		++Seed;
	}
	for (float X = -3800.f; X <= 3800.f; X += 950.f)
	{
		AddConifer(FVector(X + 300.f * (SiteRandom(Seed, 4) - 0.5f), PlazaHalfSize.Y + 1500.f + 300.f * SiteRandom(Seed, 5), 1.f), 1.0f + 0.5f * SiteRandom(Seed, 6), Seed);
		++Seed;
	}

	// По бокам площадки.
	for (const float SideX : { -PlazaHalfSize.X - 550.f, PlazaHalfSize.X + 550.f })
	{
		for (float Y = -1100.f; Y <= 1400.f; Y += 1250.f)
		{
			if (Seed % 2 == 0)
			{
				AddTree(FVector(SideX, Y, 1.f), 1.0f + 0.2f * SiteRandom(Seed, 3), Seed);
			}
			else
			{
				AddConifer(FVector(SideX, Y, 1.f), 1.1f + 0.3f * SiteRandom(Seed, 6), Seed);
			}
			++Seed;
		}
	}

	// Палисадники перед фасадом и за домом (с проходами между ними).
	const float Beds[][2] = { { -2500.f, -1400.f }, { -1250.f, -250.f }, { 250.f, 1250.f }, { 1400.f, 2500.f } };
	for (const float* Bed : Beds)
	{
		AddLawnBed(FVector2D(Bed[0], -1180.f), FVector2D(Bed[1], -840.f), Seed++);
		AddLawnBed(FVector2D(Bed[0], 700.f), FVector2D(Bed[1], 980.f), Seed++);
	}
}

void AConfiguratorSiteActor::BuildStreetFurniture()
{
	const float RoadNearY = -RoadOffset;

	for (float X = -6000.f; X <= 6000.f; X += 2000.f)
	{
		AddLamp(FVector(X + 600.f, RoadNearY + 110.f, 12.f));
	}

	// Скамейки на площадке напротив проходов между палисадниками.
	for (const float X : { -1325.f, 0.f, 1325.f })
	{
		AddBench(FVector(X, -1330.f, 4.f), /*bAlongX*/ true);
	}
	AddBench(FVector(-PlazaHalfSize.X + 350.f, 0.f, 4.f), /*bAlongX*/ false);
	AddBench(FVector(PlazaHalfSize.X - 350.f, 0.f, 4.f), /*bAlongX*/ false);
}

void AConfiguratorSiteActor::BuildDistantBuildings()
{
	// Упрощённые объёмы соседних домов (окна — процедурная сетка в материале). Стоят достаточно далеко,
	// чтобы не спорить с главным зданием, но закрывают пустой горизонт. X, Y центра; ширина, глубина; этажи.
	const float Buildings[][5] =
	{
		{ -21000.f, -20000.f, 5200.f, 1400.f, 9.f },
		{ -3500.f, -25000.f, 1800.f, 1800.f, 17.f },
		{ 14500.f, -21000.f, 4600.f, 1400.f, 12.f },
		{ 31000.f, -15000.f, 1500.f, 5200.f, 9.f },
		{ -33000.f, -7000.f, 1500.f, 4800.f, 14.f },
		{ -23000.f, 21000.f, 4800.f, 1400.f, 12.f },
		{ -1500.f, 26000.f, 5600.f, 1400.f, 9.f },
		{ 21000.f, 22000.f, 1800.f, 1800.f, 19.f },
		{ 33000.f, 8000.f, 1500.f, 4400.f, 9.f },
	};

	int32 Seed = 0;
	for (const float* Building : Buildings)
	{
		AddDistantBuilding(FVector2D(Building[0], Building[1]), FVector2D(Building[2], Building[3]), FMath::RoundToInt32(Building[4]), Seed++);
	}
}
