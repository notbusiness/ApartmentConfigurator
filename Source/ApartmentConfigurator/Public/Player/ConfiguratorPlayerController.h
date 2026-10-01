#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ConfiguratorPlayerController.generated.h"

class UConfiguratorHUDWidget;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Контроллер: превращает ввод в команды.
 *  - ЛКМ (клик)          — выбор квартиры трассировкой под курсором;
 *  - ЛКМ/ПКМ + движение  — вращение камеры вокруг точки фокуса;
 *  - колесо              — зум;
 *  - Backspace / «назад» на мыши — отмена последнего действия.
 *
 * Контроллер не меняет состояние сам: выбор квартиры уходит командой в UConfiguratorStateSubsystem,
 * вращение/зум — в пешку-камеру. Также создаёт HUD.
 */
UCLASS()
class APARTMENTCONFIGURATOR_API AConfiguratorPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AConfiguratorPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/** Класс HUD. Можно подменить на Blueprint-наследника UConfiguratorHUDWidget. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UConfiguratorHUDWidget> HUDWidgetClass;

	/**
	 * Контекст ввода. Если не задан (как в этом проекте без ассетов), действия и раскладка
	 * создаются в коде при старте — см. CreateDefaultInputAssets().
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SelectAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ZoomAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> BackAction;

	/** Градусов поворота на пиксель движения мыши. */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (ClampMin = "0.01"))
	float OrbitSensitivity = 0.25f;

	/** Смещение курсора (px), после которого нажатие ЛКМ считается перетаскиванием, а не кликом. */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (ClampMin = "0.0"))
	float DragThresholdPixels = 6.f;

private:
	void CreateDefaultInputAssets();

	void OnSelectStarted();
	void OnSelectCompleted();
	void OnLook(const FInputActionValue& Value);
	void OnZoom(const FInputActionValue& Value);
	void OnBack();

	/** Трассировка под курсором и выбор квартиры. */
	void SelectApartmentUnderCursor();

	UPROPERTY(Transient)
	TObjectPtr<UConfiguratorHUDWidget> HUDWidget;

	bool bSelectPressed = false;
	bool bDragging = false;
	float PressDragDistance = 0.f;
};
