#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Scene/SurfaceLook.h"
#include "ConfiguratorSiteActor.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPostProcessComponent;
class UStaticMesh;

/**
 * Благоустройство вокруг здания: газон, мощёная площадка, палисадники с кустами, тротуар,
 * дорога с разметкой и переходом, деревья, скамейки, фонари и соседняя застройка на горизонте.
 * Чисто декоративный актор без логики и коллизий — отделён от здания, чтобы архитектуру и
 * окружение можно было менять (или заменить реальной сценой) независимо.
 *
 * Как и архитектура, всё рисуется инстансами: по одному компоненту на пару «меш + материал».
 * Расстановка детерминированная (псевдослучайность от индекса), поэтому сцена одинакова при каждом запуске.
 * Здесь же — «атмосфера» сцены: глобальный пост-процесс (экспозиция, насыщенность, виньетка).
 */
UCLASS()
class APARTMENTCONFIGURATOR_API AConfiguratorSiteActor : public AActor
{
	GENERATED_BODY()

public:
	AConfiguratorSiteActor();

protected:
	virtual void BeginPlay() override;

	/** Полуразмеры мощёной площадки вокруг здания (см). */
	UPROPERTY(EditAnywhere, Category = "Site")
	FVector2D PlazaHalfSize = FVector2D(3800.f, 1600.f);

	/** Расстояние от центра здания до ближнего края дороги (перед фасадом). */
	UPROPERTY(EditAnywhere, Category = "Site")
	float RoadOffset = 2100.f;

	UPROPERTY(EditAnywhere, Category = "Site")
	float RoadWidth = 800.f;

	/** Рисовать ли упрощённую соседнюю застройку вдалеке (заполняет горизонт). */
	UPROPERTY(EditAnywhere, Category = "Site")
	bool bBuildDistantBuildings = true;

	/** Глобальный пост-процесс сцены (unbound): приглушает пересвет автоэкспозиции шаблонного уровня. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(EditAnywhere, Category = "Assets")
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(EditAnywhere, Category = "Assets")
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY(EditAnywhere, Category = "Assets")
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY(EditAnywhere, Category = "Assets")
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY(EditAnywhere, Category = "Assets")
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY(EditAnywhere, Category = "Assets")
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

private:
	/** Добавляет инстанс в компонент для пары (меш, внешний вид), создавая его при первом обращении. */
	void AddInstance(UStaticMesh* Mesh, const FSurfaceLook& Look, const FName& Key, const FTransform& Transform);
	void AddBox(const FSurfaceLook& Look, const FName& Key, const FVector& Min, const FVector& Max);

	/** Лиственное дерево: ствол и крона из нескольких объёмов разного размера. */
	void AddTree(const FVector& Base, float Scale, int32 Seed);
	/** Ель: ствол и ярусы конусов. */
	void AddConifer(const FVector& Base, float Scale, int32 Seed);
	void AddBush(const FVector& Base, float Scale, int32 Seed);
	void AddLamp(const FVector& Base);
	void AddBench(const FVector& Center, bool bAlongX);
	/** Палисадник: газон в бордюре с кустами. */
	void AddLawnBed(const FVector2D& Min, const FVector2D& Max, int32 Seed);
	void AddDistantBuilding(const FVector2D& Center, const FVector2D& Size, int32 Floors, int32 Seed);

	void BuildGroundAndRoad();
	void BuildPlanting();
	void BuildStreetFurniture();
	void BuildDistantBuildings();

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> Parts;
};
