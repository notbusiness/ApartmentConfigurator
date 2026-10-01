#include "Scene/BoxOutlineComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	constexpr int32 BoxEdgeCount = 12;

	/** Размер стандартного куба движка (/Engine/BasicShapes/Cube) в сантиметрах. */
	constexpr double EdgeCubeMeshSize = 100.0;
}

UBoxOutlineComponent::UBoxOutlineComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBoxOutlineComponent::Setup(UStaticMesh* InEdgeMesh, UMaterialInterface* InMaterial)
{
	EdgeMesh = InEdgeMesh;
	BaseMaterial = InMaterial;
}

void UBoxOutlineComponent::EnsureEdges()
{
	if (Edges.Num() == BoxEdgeCount || !EdgeMesh)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (BaseMaterial && !EdgeMaterial)
	{
		EdgeMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		EdgeMaterial->SetVectorParameterValue(TEXT("Color"), OutlineColor);
		EdgeMaterial->SetScalarParameterValue(TEXT("Emissive"), OutlineEmissive);
		EdgeMaterial->SetScalarParameterValue(TEXT("Opacity"), 1.f);
	}

	// Рёбра создаются один раз при первом выделении и дальше только переставляются.
	for (int32 Index = Edges.Num(); Index < BoxEdgeCount; ++Index)
	{
		UStaticMeshComponent* Edge = NewObject<UStaticMeshComponent>(Owner);
		// Рёбра переставляются в рантайме — компонент обязан быть подвижным.
		Edge->SetMobility(EComponentMobility::Movable);
		Edge->SetStaticMesh(EdgeMesh);
		if (EdgeMaterial)
		{
			Edge->SetMaterial(0, EdgeMaterial);
		}
		Edge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Edge->SetGenerateOverlapEvents(false);
		Edge->SetCastShadow(false);
		Edge->SetCanEverAffectNavigation(false);
		Edge->SetupAttachment(this);
		Edge->SetVisibility(false);
		Edge->RegisterComponent();
		Owner->AddInstanceComponent(Edge);

		Edges.Add(Edge);
	}
}

void UBoxOutlineComponent::ShowAroundBox(const FBox& LocalBox)
{
	if (!LocalBox.IsValid)
	{
		HideOutline();
		return;
	}

	EnsureEdges();
	if (Edges.Num() != BoxEdgeCount)
	{
		return;
	}

	// По X/Y рёбра выносятся наружу (не пересекаются с гранями квартиры), а по Z — утапливаются
	// на половину толщины: сверху и снизу квартиры вплотную лежат перекрытия, и вынесенные
	// горизонтальные рёбра оказались бы внутри плит или под их выступом.
	const FBox Box = LocalBox.ExpandBy(FVector(Padding, Padding, -EdgeThickness * 0.5f));
	const FVector Min = Box.Min;
	const FVector Max = Box.Max;
	const FVector Center = Box.GetCenter();
	const FVector Size = Box.GetSize();
	const double T = EdgeThickness;

	int32 EdgeIndex = 0;
	auto PlaceEdge = [this, &EdgeIndex](const FVector& Location, const FVector& Dimensions)
	{
		UStaticMeshComponent* Edge = Edges[EdgeIndex++];
		Edge->SetRelativeLocationAndRotation(Location, FRotator::ZeroRotator);
		Edge->SetRelativeScale3D(Dimensions / EdgeCubeMeshSize);
		Edge->SetVisibility(true);
	};

	// По 4 ребра вдоль каждой оси. Длина ребра увеличена на толщину, чтобы углы смыкались.
	for (const double Y : { Min.Y, Max.Y })
	{
		for (const double Z : { Min.Z, Max.Z })
		{
			PlaceEdge(FVector(Center.X, Y, Z), FVector(Size.X + T, T, T));
		}
	}
	for (const double X : { Min.X, Max.X })
	{
		for (const double Z : { Min.Z, Max.Z })
		{
			PlaceEdge(FVector(X, Center.Y, Z), FVector(T, Size.Y + T, T));
		}
	}
	for (const double X : { Min.X, Max.X })
	{
		for (const double Y : { Min.Y, Max.Y })
		{
			PlaceEdge(FVector(X, Y, Center.Z), FVector(T, T, Size.Z + T));
		}
	}
}

void UBoxOutlineComponent::HideOutline()
{
	for (UStaticMeshComponent* Edge : Edges)
	{
		if (Edge)
		{
			Edge->SetVisibility(false);
		}
	}
}
