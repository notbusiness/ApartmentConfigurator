#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "BoxOutlineComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Контур выделения: 12 рёбер параллелепипеда вокруг выбранной квартиры.
 *
 * Один общий контур на всё здание, который переставляется на выбранную квартиру,
 * вместо контура у каждой квартиры. Рёбра — кубы с полупрозрачным unlit-материалом
 * с усиленной эмиссией: за счёт bloom контур «светится». Решение не требует
 * пост-процесс материала (Custom Depth/Stencil); при переходе на пост-процесс обводку
 * интерфейс ShowAroundBox/HideOutline не изменится.
 */
UCLASS(ClassGroup = (Configurator))
class APARTMENTCONFIGURATOR_API UBoxOutlineComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UBoxOutlineComponent();

	/** Меш ребра (куб 100x100x100 с пивотом в центре) и материал с параметрами Color / Emissive / Opacity. */
	void Setup(UStaticMesh* InEdgeMesh, UMaterialInterface* InMaterial);

	/**
	 * Показать контур вокруг бокса, заданного в локальных координатах этого компонента
	 * (с небольшим отступом наружу). Рёбра — дочерние компоненты, поэтому контур
	 * поворачивается и двигается вместе с владельцем (например, с повёрнутым зданием).
	 */
	void ShowAroundBox(const FBox& LocalBox);

	void HideOutline();

	UPROPERTY(EditAnywhere, Category = "Outline", meta = (ClampMin = "1.0"))
	float EdgeThickness = 18.f;

	/** Вынос контура от боковых/фронтальной граней квартиры, чтобы рёбра не пересекались с ними (z-fighting). */
	UPROPERTY(EditAnywhere, Category = "Outline", meta = (ClampMin = "0.0"))
	float Padding = 10.f;

	/** Контрастный к заливке выбранной квартиры (золотой) и к цветам статусов. */
	UPROPERTY(EditAnywhere, Category = "Outline")
	FLinearColor OutlineColor = FLinearColor(0.02f, 0.85f, 1.f);

	/** Сила свечения: больше 1 — контур светится (bloom). */
	UPROPERTY(EditAnywhere, Category = "Outline", meta = (ClampMin = "0.0"))
	float OutlineEmissive = 2.5f;

private:
	void EnsureEdges();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Edges;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> EdgeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> EdgeMaterial;
};
