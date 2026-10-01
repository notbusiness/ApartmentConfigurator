#include "Scene/SurfaceLook.h"

#include "Materials/MaterialInstanceDynamic.h"

void FSurfaceLook::ApplyTo(UMaterialInstanceDynamic& Material, const FTransform& ActorTransform) const
{
	Material.SetVectorParameterValue(TEXT("Color"), Color);
	Material.SetScalarParameterValue(TEXT("Roughness"), Roughness);
	Material.SetScalarParameterValue(TEXT("Metallic"), Metallic);
	Material.SetScalarParameterValue(TEXT("Pattern"), static_cast<float>(Pattern));

	// Пространство актора для процедурного рисунка: начало координат и поворот вокруг вертикали.
	const FVector Origin = ActorTransform.GetLocation();
	Material.SetVectorParameterValue(TEXT("Origin"), FLinearColor(static_cast<float>(Origin.X), static_cast<float>(Origin.Y), static_cast<float>(Origin.Z), 0.f));
	Material.SetScalarParameterValue(TEXT("Yaw"), FMath::DegreesToRadians(static_cast<float>(ActorTransform.Rotator().Yaw)));
}
