#include "Scene/BuildingPlan.h"

#include "ConfiguratorDataTypes.h"

namespace
{
	/** Объём подсветки чуть больше квартиры, чтобы не мерцать на одной плоскости с фасадом (z-fighting). */
	constexpr float VolumeOutset = 8.f;

	/** Зазор между объёмами соседних квартир — подсветки соседей визуально не сливаются. */
	constexpr float VolumeSideInset = 6.f;

	/** Балкон у двух квартир из трёх, начиная со второго этажа: фасад выглядит живым, а не штампованным. */
	bool ShouldHaveBalcony(int32 FloorIndex, int32 IndexOnFloor)
	{
		return FloorIndex >= 1 && (FloorIndex + IndexOnFloor) % 3 != 2;
	}
}

FBuildingPlan FBuildingPlan::Make(const FBuildingData& Data, const FBuildingLayout& InLayout)
{
	FBuildingPlan Plan;
	Plan.Layout = InLayout;
	const FBuildingLayout& Layout = Plan.Layout;

	int32 MaxApartmentsOnFloor = 1;
	for (const FFloorData& Floor : Data.Floors)
	{
		MaxApartmentsOnFloor = FMath::Max(MaxApartmentsOnFloor, Floor.Apartments.Num());
	}

	const float BuildingWidth = MaxApartmentsOnFloor * Layout.NominalApartmentWidth;
	Plan.MinX = -BuildingWidth * 0.5f;
	Plan.MaxX = BuildingWidth * 0.5f;
	Plan.FloorCount = Data.Floors.Num();
	Plan.TopZ = Plan.FloorCount * Layout.FloorHeight;

	const float FrontY = Plan.GetFrontY();
	const float BackY = Plan.GetBackY();

	for (int32 FloorIndex = 0; FloorIndex < Data.Floors.Num(); ++FloorIndex)
	{
		const TArray<FApartmentData>& Apartments = Data.Floors[FloorIndex].Apartments;
		const float BaseZ = Plan.GetFloorBaseZ(FloorIndex);

		// Этаж без квартир (например, технический) всё равно получает фасад — из одинаковых декоративных модулей.
		if (Apartments.IsEmpty())
		{
			const int32 Count = FMath::Max(1, FMath::RoundToInt32(BuildingWidth / Layout.NominalApartmentWidth));
			for (int32 Index = 0; Index < Count; ++Index)
			{
				FFacadeModulePlan& Module = Plan.Modules.AddDefaulted_GetRef();
				Module.FloorIndex = FloorIndex;
				Module.MinX = Plan.MinX + BuildingWidth * Index / Count;
				Module.MaxX = Plan.MinX + BuildingWidth * (Index + 1) / Count;
			}
			continue;
		}

		// Доли ширины пропорциональны площади; неизвестная площадь считается средней.
		float KnownAreaSum = 0.f;
		int32 KnownAreaCount = 0;
		for (const FApartmentData& Apartment : Apartments)
		{
			if (Apartment.Area > 0.f)
			{
				KnownAreaSum += Apartment.Area;
				++KnownAreaCount;
			}
		}
		const float AverageArea = KnownAreaCount > 0 ? KnownAreaSum / KnownAreaCount : 1.f;

		TArray<float> Shares;
		Shares.Reserve(Apartments.Num());
		float ShareSum = 0.f;
		for (const FApartmentData& Apartment : Apartments)
		{
			const float Area = Apartment.Area > 0.f ? Apartment.Area : AverageArea;
			const float Share = FMath::Max(Area, AverageArea * Layout.MinWidthShare);
			Shares.Add(Share);
			ShareSum += Share;
		}

		float X = Plan.MinX;
		for (int32 Index = 0; Index < Apartments.Num(); ++Index)
		{
			const bool bLast = Index == Apartments.Num() - 1;
			const float NextX = bLast ? Plan.MaxX : X + BuildingWidth * Shares[Index] / ShareSum;
			const bool bBalcony = ShouldHaveBalcony(FloorIndex, Index);

			FFacadeModulePlan& Module = Plan.Modules.AddDefaulted_GetRef();
			Module.FloorIndex = FloorIndex;
			Module.MinX = X;
			Module.MaxX = NextX;
			Module.bHasBalcony = bBalcony;
			Module.ApartmentIndex = Plan.Apartments.Num();

			FApartmentPlan& ApartmentPlan = Plan.Apartments.AddDefaulted_GetRef();
			ApartmentPlan.Id = Apartments[Index].Id;
			ApartmentPlan.FloorIndex = FloorIndex;
			ApartmentPlan.ModuleIndex = Plan.Modules.Num() - 1;

			const float FrontOfVolume = FrontY - (bBalcony ? Layout.BalconyDepth : 0.f) - VolumeOutset;
			ApartmentPlan.VolumeBox = FBox(
				FVector(X + VolumeSideInset, FrontOfVolume, BaseZ + Layout.SlabThickness + 2.f),
				FVector(NextX - VolumeSideInset, BackY + VolumeOutset, BaseZ + Layout.FloorHeight - 2.f));

			ApartmentPlan.DefaultFocus = FVector(
				Module.GetCenterX(),
				FrontY - 60.f,
				BaseZ + (Layout.SlabThickness + Layout.FloorHeight) * 0.5f);

			X = NextX;
		}
	}

	Plan.Bounds = FBox(
		FVector(Plan.MinX, FrontY - Layout.BalconyDepth, 0.f),
		FVector(Plan.MaxX, BackY, FMath::Max(Plan.TopZ, Layout.FloorHeight)));
	return Plan;
}
