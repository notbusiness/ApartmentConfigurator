#include "Scene/BuildingArchitectureComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Scene/BuildingPlan.h"

namespace
{
	constexpr double ArchitectureCubeSize = 100.0;

	/** Толщина наружных стен. */
	constexpr float WallThickness = 30.f;

	/** Вынос перекрытий за плоскость фасада — горизонтальные «пояса» между этажами. */
	constexpr float SlabOverhang = 12.f;

	/** Шаг и размеры элементов ограждения балконов. */
	constexpr float RailingHeight = 105.f;
	constexpr float BalusterStep = 12.f;
	constexpr float BalusterSize = 2.5f;

	const TCHAR* GetPartName(EArchitecturePart Part)
	{
		switch (Part)
		{
		case EArchitecturePart::Wall:		return TEXT("Arch_Wall");
		case EArchitecturePart::WallAlt:	return TEXT("Arch_WallAlt");
		case EArchitecturePart::Accent:		return TEXT("Arch_Accent");
		case EArchitecturePart::Glass:		return TEXT("Arch_Glass");
		case EArchitecturePart::Frame:		return TEXT("Arch_Frame");
		case EArchitecturePart::Concrete:	return TEXT("Arch_Concrete");
		case EArchitecturePart::Metal:		return TEXT("Arch_Metal");
		case EArchitecturePart::Trim:		return TEXT("Arch_Trim");
		case EArchitecturePart::Plinth:		return TEXT("Arch_Plinth");
		default:							return TEXT("Arch_Roof");
		}
	}
}

UBuildingArchitectureComponent::UBuildingArchitectureComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Палитра «современный жилой дом»: тёплая штукатурка и кирпичные секции, графитовые акценты,
	// голубоватое стекло с отражением неба (металличность > 0 даёт зеркальность, цвет — оттенок отражения).
	WallLook = FSurfaceLook(FLinearColor(0.50f, 0.45f, 0.38f), 0.9f, 0.f, ESurfacePattern::Plaster);
	AltWallLook = FSurfaceLook(FLinearColor(0.36f, 0.16f, 0.09f), 0.85f, 0.f, ESurfacePattern::Brick);
	AccentLook = FSurfaceLook(FLinearColor(0.07f, 0.07f, 0.08f), 0.6f);
	GlassLook = FSurfaceLook(FLinearColor(0.16f, 0.22f, 0.27f), 0.04f, 0.65f, ESurfacePattern::Glass);
	FrameLook = FSurfaceLook(FLinearColor(0.03f, 0.03f, 0.035f), 0.4f, 0.5f);
	ConcreteLook = FSurfaceLook(FLinearColor(0.40f, 0.40f, 0.38f), 0.85f, 0.f, ESurfacePattern::Concrete);
	MetalLook = FSurfaceLook(FLinearColor(0.07f, 0.07f, 0.08f), 0.35f, 0.9f);
	TrimLook = FSurfaceLook(FLinearColor(0.58f, 0.58f, 0.56f), 0.5f);
	PlinthLook = FSurfaceLook(FLinearColor(0.15f, 0.14f, 0.13f), 0.8f, 0.f, ESurfacePattern::Concrete);
	RoofLook = FSurfaceLook(FLinearColor(0.20f, 0.20f, 0.21f), 0.9f, 0.f, ESurfacePattern::Asphalt);
}

// ---------------------------------------------------------------- Инстансы

const FSurfaceLook& UBuildingArchitectureComponent::GetLook(EArchitecturePart Part) const
{
	switch (Part)
	{
	case EArchitecturePart::Wall:		return WallLook;
	case EArchitecturePart::WallAlt:	return AltWallLook;
	case EArchitecturePart::Accent:		return AccentLook;
	case EArchitecturePart::Glass:		return GlassLook;
	case EArchitecturePart::Frame:		return FrameLook;
	case EArchitecturePart::Concrete:	return ConcreteLook;
	case EArchitecturePart::Metal:		return MetalLook;
	case EArchitecturePart::Trim:		return TrimLook;
	case EArchitecturePart::Plinth:		return PlinthLook;
	default:							return RoofLook;
	}
}

UInstancedStaticMeshComponent* UBuildingArchitectureComponent::GetOrCreatePartComponent(EArchitecturePart Part)
{
	const int32 Index = static_cast<int32>(Part);
	if (PartComponents.IsValidIndex(Index) && PartComponents[Index])
	{
		return PartComponents[Index];
	}

	AActor* Owner = GetOwner();
	if (!Owner || !CubeMesh)
	{
		return nullptr;
	}

	PartComponents.SetNum(static_cast<int32>(EArchitecturePart::Count));
	PartMaterials.SetNum(static_cast<int32>(EArchitecturePart::Count));

	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(Owner, GetPartName(Part));
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(CubeMesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);

	if (SurfaceMaterial)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(SurfaceMaterial, this);
		GetLook(Part).ApplyTo(*Material, Owner->GetActorTransform());
		Component->SetMaterial(0, Material);
		PartMaterials[Index] = Material;
	}

	Component->SetupAttachment(this);
	Component->RegisterComponent();
	Owner->AddInstanceComponent(Component);

	PartComponents[Index] = Component;
	return Component;
}

void UBuildingArchitectureComponent::AddBox(EArchitecturePart Part, const FVector& Min, const FVector& Max)
{
	const FVector Size = (Max - Min).GetAbs();
	if (Size.X < UE_KINDA_SMALL_NUMBER || Size.Y < UE_KINDA_SMALL_NUMBER || Size.Z < UE_KINDA_SMALL_NUMBER)
	{
		return;
	}
	PendingInstances[static_cast<int32>(Part)].Emplace(FQuat::Identity, (Min + Max) * 0.5, Size / ArchitectureCubeSize);
}

void UBuildingArchitectureComponent::Clear()
{
	for (UInstancedStaticMeshComponent* Component : PartComponents)
	{
		if (Component)
		{
			Component->ClearInstances();
		}
	}
}

// ---------------------------------------------------------------- Сборка

void UBuildingArchitectureComponent::Build(const FBuildingPlan& Plan)
{
	Clear();

	PendingInstances.Reset();
	PendingInstances.SetNum(static_cast<int32>(EArchitecturePart::Count));

	for (int32 FloorIndex = 0; FloorIndex < Plan.FloorCount; ++FloorIndex)
	{
		BuildFloorShell(Plan, FloorIndex);
	}
	for (const FFacadeModulePlan& Module : Plan.Modules)
	{
		BuildFacadeModule(Plan, Module, /*bFront*/ true);
		BuildFacadeModule(Plan, Module, /*bFront*/ false);
	}
	BuildRoof(Plan);
	BuildBaseAndEntrance(Plan);

	// Один пакетный вызов на материал вместо тысяч отдельных AddInstance.
	for (int32 PartIndex = 0; PartIndex < PendingInstances.Num(); ++PartIndex)
	{
		if (PendingInstances[PartIndex].IsEmpty())
		{
			continue;
		}
		if (UInstancedStaticMeshComponent* Component = GetOrCreatePartComponent(static_cast<EArchitecturePart>(PartIndex)))
		{
			Component->AddInstances(PendingInstances[PartIndex], /*bShouldReturnIndices*/ false, /*bWorldSpace*/ false);
		}
	}
	PendingInstances.Reset();
}

void UBuildingArchitectureComponent::BuildFloorShell(const FBuildingPlan& Plan, int32 FloorIndex)
{
	const FBuildingLayout& Layout = Plan.Layout;
	const float BaseZ = Plan.GetFloorBaseZ(FloorIndex);
	const float FrontY = Plan.GetFrontY();
	const float BackY = Plan.GetBackY();

	// Перекрытие с небольшим выносом — горизонтальные пояса на фасаде.
	AddBox(EArchitecturePart::Concrete,
		FVector(Plan.MinX - SlabOverhang, FrontY - SlabOverhang, BaseZ),
		FVector(Plan.MaxX + SlabOverhang, BackY + SlabOverhang, BaseZ + Layout.SlabThickness));

	// Глухие торцевые стены.
	const float WallMinZ = BaseZ + Layout.SlabThickness;
	const float WallMaxZ = BaseZ + Layout.FloorHeight;
	AddBox(EArchitecturePart::Wall, FVector(Plan.MinX, FrontY, WallMinZ), FVector(Plan.MinX + WallThickness, BackY, WallMaxZ));
	AddBox(EArchitecturePart::Wall, FVector(Plan.MaxX - WallThickness, FrontY, WallMinZ), FVector(Plan.MaxX, BackY, WallMaxZ));

	// Графитовые угловые пилястры подчёркивают объём корпуса.
	for (const float CornerX : { Plan.MinX, Plan.MaxX })
	{
		for (const float CornerY : { FrontY, BackY })
		{
			AddBox(EArchitecturePart::Accent,
				FVector(CornerX - 14.f, CornerY - 14.f, WallMinZ),
				FVector(CornerX + 14.f, CornerY + 14.f, WallMaxZ));
		}
	}
}

void UBuildingArchitectureComponent::BuildFacadeModule(const FBuildingPlan& Plan, const FFacadeModulePlan& Module, bool bFront)
{
	const FBuildingLayout& Layout = Plan.Layout;
	const float BaseZ = Plan.GetFloorBaseZ(Module.FloorIndex);
	const float FloorZ = BaseZ + Layout.SlabThickness;
	const float CeilingZ = BaseZ + Layout.FloorHeight;
	const float Width = Module.GetWidth();
	const float CenterX = Module.GetCenterX();

	// Наружная плоскость фасада и направление «внутрь здания» по Y.
	const float OuterY = bFront ? Plan.GetFrontY() : Plan.GetBackY();
	const float InwardSign = bFront ? 1.f : -1.f;

	// Стены модуля не заходят на торцевые стены здания.
	const float MinX = FMath::Max(Module.MinX, Plan.MinX + WallThickness);
	const float MaxX = FMath::Min(Module.MaxX, Plan.MaxX - WallThickness);

	TArray<FOpening> Openings;
	if (bFront && Module.bHasBalcony)
	{
		// Панорамное окно-дверь на балкон.
		const float WindowWidth = FMath::Clamp(Width * 0.6f, 240.f, 640.f);
		Openings.Add({ CenterX - WindowWidth * 0.5f, CenterX + WindowWidth * 0.5f, FloorZ, FloorZ + 245.f });
	}
	else
	{
		// Два окна с подоконной частью.
		const float WindowWidth = FMath::Clamp(Width * 0.24f, 110.f, 240.f);
		for (const float Fraction : { 0.27f, 0.73f })
		{
			const float WindowCenter = Module.MinX + Width * Fraction;
			Openings.Add({ WindowCenter - WindowWidth * 0.5f, WindowCenter + WindowWidth * 0.5f, FloorZ + 85.f, FloorZ + 240.f });
		}
	}

	// Второй тон — у «вертикальных полос» фасада: секция берёт цвет по своему положению вдоль корпуса,
	// поэтому на соседних этажах цвета в основном совпадают, а из-за разной ширины квартир
	// получается живая «мозаика», а не шахматка.
	const int32 Column = FMath::RoundToInt32((CenterX - Plan.MinX) / Layout.NominalApartmentWidth);
	const EArchitecturePart WallPart = (Column % 3 == 1) ? EArchitecturePart::WallAlt : EArchitecturePart::Wall;

	BuildWallWithOpenings(WallPart, MinX, MaxX, FloorZ, CeilingZ, OuterY, InwardSign, Openings);
	for (int32 OpeningIndex = 0; OpeningIndex < Openings.Num(); ++OpeningIndex)
	{
		const FOpening& Opening = Openings[OpeningIndex];
		BuildWindow(Opening, OuterY, InwardSign);

		// Наружный блок кондиционера под частью окон с подоконником: мелкая «жилая» деталь,
		// без которой фасад выглядит макетом. Расстановка детерминированная (по положению окна).
		// На первом этаже блоков нет: на уровне глаз и в досягаемости с тротуара их не вешают.
		const bool bSillWindow = Opening.MinZ > FloorZ + 1.f;
		const bool bAboveGroundFloor = Module.FloorIndex > 0;
		const int32 Seed = Module.FloorIndex * 7 + FMath::RoundToInt32(Opening.MinX / 50.f) + (bFront ? 0 : 3);
		if (bSillWindow && bAboveGroundFloor && ((Seed % 4) + 4) % 4 == 1)
		{
			BuildAirConditioner(Opening, OuterY, InwardSign);
		}
	}

	if (bFront && Module.bHasBalcony)
	{
		const float BalconyMinX = FMath::Max(Openings[0].MinX - 40.f, Module.MinX + 25.f);
		const float BalconyMaxX = FMath::Min(Openings[0].MaxX + 40.f, Module.MaxX - 25.f);
		BuildBalcony(BalconyMinX, BalconyMaxX, FloorZ, OuterY, Layout.BalconyDepth);
	}

	// Пилястра на границе квартир: визуально делит фасад на квартиры, как в реальных проектах.
	if (Module.MinX > Plan.MinX + 1.f)
	{
		const float Outward = -InwardSign * 10.f;
		AddBox(EArchitecturePart::Accent,
			FVector(Module.MinX - 12.f, FMath::Min(OuterY + Outward, OuterY + InwardSign * WallThickness), FloorZ),
			FVector(Module.MinX + 12.f, FMath::Max(OuterY + Outward, OuterY + InwardSign * WallThickness), CeilingZ));
	}
}

void UBuildingArchitectureComponent::BuildWallWithOpenings(EArchitecturePart WallPart, float MinX, float MaxX, float MinZ, float MaxZ, float OuterY, float InwardSign, TArray<FOpening> Openings)
{
	const float WallMinY = FMath::Min(OuterY, OuterY + InwardSign * WallThickness);
	const float WallMaxY = FMath::Max(OuterY, OuterY + InwardSign * WallThickness);

	Openings.Sort([](const FOpening& A, const FOpening& B) { return A.MinX < B.MinX; });

	// Стена = вертикальные простенки между проёмами + участки над и под каждым проёмом.
	float CursorX = MinX;
	for (const FOpening& Opening : Openings)
	{
		const float OpeningMinX = FMath::Clamp(Opening.MinX, MinX, MaxX);
		const float OpeningMaxX = FMath::Clamp(Opening.MaxX, MinX, MaxX);

		AddBox(WallPart, FVector(CursorX, WallMinY, MinZ), FVector(OpeningMinX, WallMaxY, MaxZ));
		AddBox(WallPart, FVector(OpeningMinX, WallMinY, MinZ), FVector(OpeningMaxX, WallMaxY, Opening.MinZ));
		AddBox(WallPart, FVector(OpeningMinX, WallMinY, Opening.MaxZ), FVector(OpeningMaxX, WallMaxY, MaxZ));
		CursorX = OpeningMaxX;
	}
	AddBox(WallPart, FVector(CursorX, WallMinY, MinZ), FVector(MaxX, WallMaxY, MaxZ));
}

void UBuildingArchitectureComponent::BuildWindow(const FOpening& Opening, float OuterY, float InwardSign)
{
	// Y-диапазон слоя на заданной глубине от наружной плоскости фасада.
	auto LayerY = [OuterY, InwardSign](float FromDepth, float ToDepth)
	{
		const float A = OuterY + InwardSign * FromDepth;
		const float B = OuterY + InwardSign * ToDepth;
		return FVector2f(FMath::Min(A, B), FMath::Max(A, B));
	};

	const FVector2f GlassY = LayerY(12.f, 14.f);
	const FVector2f FrameY = LayerY(6.f, 15.f);
	const float Bar = 6.f;

	// Стекло (утоплено в проём).
	AddBox(EArchitecturePart::Glass,
		FVector(Opening.MinX, GlassY.X, Opening.MinZ),
		FVector(Opening.MaxX, GlassY.Y, Opening.MaxZ));

	// Рама по периметру.
	AddBox(EArchitecturePart::Frame, FVector(Opening.MinX, FrameY.X, Opening.MinZ), FVector(Opening.MinX + Bar, FrameY.Y, Opening.MaxZ));
	AddBox(EArchitecturePart::Frame, FVector(Opening.MaxX - Bar, FrameY.X, Opening.MinZ), FVector(Opening.MaxX, FrameY.Y, Opening.MaxZ));
	AddBox(EArchitecturePart::Frame, FVector(Opening.MinX, FrameY.X, Opening.MaxZ - Bar), FVector(Opening.MaxX, FrameY.Y, Opening.MaxZ));
	AddBox(EArchitecturePart::Frame, FVector(Opening.MinX, FrameY.X, Opening.MinZ), FVector(Opening.MaxX, FrameY.Y, Opening.MinZ + Bar));

	// Импосты: вертикальные — примерно через 1.4 м, горизонтальная фрамуга — у высоких окон.
	const float Width = Opening.MaxX - Opening.MinX;
	const int32 Panes = FMath::Max(1, FMath::RoundToInt32(Width / 140.f));
	for (int32 Pane = 1; Pane < Panes; ++Pane)
	{
		const float X = Opening.MinX + Width * Pane / Panes;
		AddBox(EArchitecturePart::Frame, FVector(X - Bar * 0.5f, FrameY.X, Opening.MinZ), FVector(X + Bar * 0.5f, FrameY.Y, Opening.MaxZ));
	}
	if (Opening.MaxZ - Opening.MinZ > 200.f)
	{
		const float TransomZ = Opening.MinZ + (Opening.MaxZ - Opening.MinZ) * 0.78f;
		AddBox(EArchitecturePart::Frame, FVector(Opening.MinX, FrameY.X, TransomZ - Bar * 0.5f), FVector(Opening.MaxX, FrameY.Y, TransomZ + Bar * 0.5f));
	}
	else
	{
		// Отлив под окном с подоконной частью.
		const FVector2f SillY = LayerY(-6.f, 4.f);
		AddBox(EArchitecturePart::Concrete, FVector(Opening.MinX - 5.f, SillY.X, Opening.MinZ - 5.f), FVector(Opening.MaxX + 5.f, SillY.Y, Opening.MinZ));
	}
}

void UBuildingArchitectureComponent::BuildAirConditioner(const FOpening& Window, float OuterY, float InwardSign)
{
	// Корпус блока и тёмная решётка вентилятора на лицевой стороне; висит под окном у его края.
	const float Outward = -InwardSign;
	const float MinX = Window.MinX + 8.f;
	const float MaxX = MinX + 78.f;
	const float MaxZ = Window.MinZ - 14.f;
	const float MinZ = MaxZ - 52.f;

	const float FaceY = OuterY + Outward * 30.f;
	AddBox(EArchitecturePart::Trim,
		FVector(MinX, FMath::Min(OuterY, FaceY), MinZ),
		FVector(MaxX, FMath::Max(OuterY, FaceY), MaxZ));

	const float GrilleY = FaceY + Outward * 1.5f;
	AddBox(EArchitecturePart::Frame,
		FVector(MinX + 8.f, FMath::Min(FaceY, GrilleY), MinZ + 6.f),
		FVector(MinX + 50.f, FMath::Max(FaceY, GrilleY), MaxZ - 6.f));
}

void UBuildingArchitectureComponent::BuildBalcony(float MinX, float MaxX, float FloorZ, float FacadeY, float Depth)
{
	const float FrontY = FacadeY - Depth;

	// Плита балкона: верх вровень с полом квартиры.
	AddBox(EArchitecturePart::Concrete, FVector(MinX, FrontY, FloorZ - 18.f), FVector(MaxX, FacadeY, FloorZ));

	// Ограждение: поручень, нижний прогон и стойки-балясины по фронту и бокам.
	const float TopZ = FloorZ + RailingHeight;
	AddBox(EArchitecturePart::Metal, FVector(MinX, FrontY, TopZ - 5.f), FVector(MaxX, FrontY + 5.f, TopZ));
	AddBox(EArchitecturePart::Metal, FVector(MinX, FrontY, FloorZ + 8.f), FVector(MaxX, FrontY + 4.f, FloorZ + 12.f));
	for (float X = MinX + BalusterStep * 0.5f; X < MaxX; X += BalusterStep)
	{
		AddBox(EArchitecturePart::Metal, FVector(X, FrontY + 1.f, FloorZ + 12.f), FVector(X + BalusterSize, FrontY + 1.f + BalusterSize, TopZ - 5.f));
	}

	for (const float SideX : { MinX, MaxX - 5.f })
	{
		AddBox(EArchitecturePart::Metal, FVector(SideX, FrontY, TopZ - 5.f), FVector(SideX + 5.f, FacadeY, TopZ));
		for (float Y = FrontY + BalusterStep; Y < FacadeY; Y += BalusterStep)
		{
			AddBox(EArchitecturePart::Metal, FVector(SideX + 1.f, Y, FloorZ), FVector(SideX + 1.f + BalusterSize, Y + BalusterSize, TopZ - 5.f));
		}
	}
}

void UBuildingArchitectureComponent::BuildRoof(const FBuildingPlan& Plan)
{
	const float FrontY = Plan.GetFrontY();
	const float BackY = Plan.GetBackY();
	const float RoofZ = Plan.TopZ;
	const float RoofTopZ = RoofZ + 35.f;
	const float ParapetZ = RoofTopZ + 110.f;
	const float Edge = SlabOverhang;

	AddBox(EArchitecturePart::Roof, FVector(Plan.MinX - Edge, FrontY - Edge, RoofZ), FVector(Plan.MaxX + Edge, BackY + Edge, RoofTopZ));

	// Парапет по периметру.
	const float T = 25.f;
	AddBox(EArchitecturePart::Accent, FVector(Plan.MinX - Edge, FrontY - Edge, RoofTopZ), FVector(Plan.MaxX + Edge, FrontY - Edge + T, ParapetZ));
	AddBox(EArchitecturePart::Accent, FVector(Plan.MinX - Edge, BackY + Edge - T, RoofTopZ), FVector(Plan.MaxX + Edge, BackY + Edge, ParapetZ));
	AddBox(EArchitecturePart::Accent, FVector(Plan.MinX - Edge, FrontY - Edge, RoofTopZ), FVector(Plan.MinX - Edge + T, BackY + Edge, ParapetZ));
	AddBox(EArchitecturePart::Accent, FVector(Plan.MaxX + Edge - T, FrontY - Edge, RoofTopZ), FVector(Plan.MaxX + Edge, BackY + Edge, ParapetZ));

	// Светлая крышка парапета с небольшим свесом.
	const float Cap = 6.f;
	AddBox(EArchitecturePart::Trim, FVector(Plan.MinX - Edge - Cap, FrontY - Edge - Cap, ParapetZ), FVector(Plan.MaxX + Edge + Cap, FrontY - Edge + T + Cap, ParapetZ + 5.f));
	AddBox(EArchitecturePart::Trim, FVector(Plan.MinX - Edge - Cap, BackY + Edge - T - Cap, ParapetZ), FVector(Plan.MaxX + Edge + Cap, BackY + Edge + Cap, ParapetZ + 5.f));
	AddBox(EArchitecturePart::Trim, FVector(Plan.MinX - Edge - Cap, FrontY - Edge - Cap, ParapetZ), FVector(Plan.MinX - Edge + T + Cap, BackY + Edge + Cap, ParapetZ + 5.f));
	AddBox(EArchitecturePart::Trim, FVector(Plan.MaxX + Edge - T - Cap, FrontY - Edge - Cap, ParapetZ), FVector(Plan.MaxX + Edge + Cap, BackY + Edge + Cap, ParapetZ + 5.f));

	// Выход на кровлю / машинное помещение лифта и вентиляционное оборудование.
	const float HalfDepth = Plan.Layout.Depth * 0.5f;
	AddBox(EArchitecturePart::Wall, FVector(-320.f, BackY - HalfDepth - 150.f, RoofTopZ), FVector(320.f, BackY - 120.f, RoofTopZ + 330.f));
	AddBox(EArchitecturePart::Roof, FVector(-340.f, BackY - HalfDepth - 170.f, RoofTopZ + 330.f), FVector(340.f, BackY - 100.f, RoofTopZ + 350.f));

	const float Width = Plan.MaxX - Plan.MinX;
	for (const float Fraction : { 0.18f, 0.72f, 0.84f })
	{
		const float X = Plan.MinX + Width * Fraction;
		AddBox(EArchitecturePart::Metal, FVector(X - 80.f, -90.f, RoofTopZ), FVector(X + 80.f, 60.f, RoofTopZ + 110.f));
	}
}

void UBuildingArchitectureComponent::BuildBaseAndEntrance(const FBuildingPlan& Plan)
{
	const float FrontY = Plan.GetFrontY();
	const float BackY = Plan.GetBackY();
	const float FloorZ = Plan.Layout.SlabThickness;

	// Цоколь: тёмный пояс по периметру, чуть шире корпуса.
	AddBox(EArchitecturePart::Plinth, FVector(Plan.MinX - 20.f, FrontY - 20.f, -30.f), FVector(Plan.MaxX + 20.f, BackY + 20.f, FloorZ + 5.f));

	// Входная группа на левом торце: стеклянные двери в раме и козырёк.
	const float DoorX = Plan.MinX - 4.f;
	AddBox(EArchitecturePart::Glass, FVector(DoorX, -110.f, FloorZ), FVector(Plan.MinX, 110.f, FloorZ + 240.f));
	AddBox(EArchitecturePart::Frame, FVector(DoorX - 4.f, -116.f, FloorZ), FVector(Plan.MinX, -110.f, FloorZ + 246.f));
	AddBox(EArchitecturePart::Frame, FVector(DoorX - 4.f, 110.f, FloorZ), FVector(Plan.MinX, 116.f, FloorZ + 246.f));
	AddBox(EArchitecturePart::Frame, FVector(DoorX - 4.f, -116.f, FloorZ + 240.f), FVector(Plan.MinX, 116.f, FloorZ + 246.f));
	AddBox(EArchitecturePart::Frame, FVector(DoorX - 4.f, -3.f, FloorZ), FVector(Plan.MinX, 3.f, FloorZ + 240.f));
	AddBox(EArchitecturePart::Concrete, FVector(Plan.MinX - 170.f, -230.f, FloorZ + 262.f), FVector(Plan.MinX, 230.f, FloorZ + 280.f));
}
