#include "Scene/ApartmentMeshComponent.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const FName OverlayColorParam(TEXT("Color"));
	const FName OverlayOpacityParam(TEXT("Opacity"));
}

UApartmentMeshComponent::UApartmentMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Нужны только запросы (трассировка клика), физика не нужна.
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldStatic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);

	// Подсветка — полупрозрачный объём: тени от него были бы артефактом.
	CastShadow = false;
}

void UApartmentMeshComponent::InitializeApartment(const FApartmentData& Data, int32 InFloorIndex, UMaterialInterface* InOverlayMaterial, float Emissive)
{
	ApartmentId = Data.Id;
	FloorIndex = InFloorIndex;
	Status = Data.Status;
	AppliedOpacity = -1.f;

	if (InOverlayMaterial)
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(InOverlayMaterial, this);
		DynamicMaterial->SetScalarParameterValue(TEXT("Emissive"), Emissive);
		SetMaterial(0, DynamicMaterial);
	}
}

void UApartmentMeshComponent::ApplyOverlay(const FLinearColor& Color, float Opacity, bool bClickable)
{
	if (AppliedOpacity >= 0.f && AppliedColor.Equals(Color) && FMath::IsNearlyEqual(AppliedOpacity, Opacity) && bAppliedClickable == bClickable)
	{
		return;
	}
	AppliedColor = Color;
	AppliedOpacity = Opacity;
	bAppliedClickable = bClickable;

	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(OverlayColorParam, Color);
		DynamicMaterial->SetScalarParameterValue(OverlayOpacityParam, Opacity);
	}

	// Прозрачный объём не рисуем вовсе (экономия на полупрозрачности), но клик по нему работает.
	SetVisibility(Opacity > 0.001f);

	// Скрытые фильтром квартиры не должны перехватывать клик — так же, как неактивные кнопки в панели.
	SetCollisionResponseToChannel(ECC_Visibility, bClickable ? ECR_Block : ECR_Ignore);
}
