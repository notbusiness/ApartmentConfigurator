#include "Player/ConfiguratorPlayerController.h"

#include "ApartmentConfigurator.h"
#include "Blueprint/UserWidget.h"
#include "Camera/ConfiguratorCameraPawn.h"
#include "Core/ConfiguratorStateSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Scene/ApartmentMeshComponent.h"
#include "UI/ConfiguratorHUDWidget.h"

namespace
{
	UInputAction* MakeInputAction(UObject* Outer, const TCHAR* Name, EInputActionValueType ValueType)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = ValueType;
		return Action;
	}
}

AConfiguratorPlayerController::AConfiguratorPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;	// клики обрабатываются через Enhanced Input + собственную трассировку
	bEnableMouseOverEvents = false;	// hover по мешам не нужен — не тратим трассировку каждый кадр
	DefaultMouseCursor = EMouseCursor::Default;

	HUDWidgetClass = UConfiguratorHUDWidget::StaticClass();
}

void AConfiguratorPlayerController::CreateDefaultInputAssets()
{
	// В реальном проекте это были бы ассеты IA_*/IMC_* (их можно назначить в свойствах контроллера).
	// Здесь они создаются в коде, чтобы проект не содержал бинарных файлов и весь ввод был виден в исходниках.
	if (!SelectAction)
	{
		SelectAction = MakeInputAction(this, TEXT("IA_Select"), EInputActionValueType::Boolean);
	}
	if (!LookAction)
	{
		LookAction = MakeInputAction(this, TEXT("IA_Look"), EInputActionValueType::Axis2D);
	}
	if (!ZoomAction)
	{
		ZoomAction = MakeInputAction(this, TEXT("IA_Zoom"), EInputActionValueType::Axis1D);
	}
	if (!BackAction)
	{
		BackAction = MakeInputAction(this, TEXT("IA_Back"), EInputActionValueType::Boolean);
	}

	if (!MappingContext)
	{
		MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Configurator"));
		MappingContext->MapKey(SelectAction, EKeys::LeftMouseButton);
		MappingContext->MapKey(LookAction, EKeys::Mouse2D);
		MappingContext->MapKey(ZoomAction, EKeys::MouseWheelAxis);
		MappingContext->MapKey(BackAction, EKeys::BackSpace);
		MappingContext->MapKey(BackAction, EKeys::ThumbMouseButton);
	}
}

void AConfiguratorPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	CreateDefaultInputAssets();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogConfigurator, Error, TEXT("Enhanced Input component is required (check DefaultInputComponentClass in DefaultInput.ini)"));
		return;
	}

	EnhancedInput->BindAction(SelectAction, ETriggerEvent::Started, this, &AConfiguratorPlayerController::OnSelectStarted);
	EnhancedInput->BindAction(SelectAction, ETriggerEvent::Completed, this, &AConfiguratorPlayerController::OnSelectCompleted);
	EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AConfiguratorPlayerController::OnLook);
	EnhancedInput->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AConfiguratorPlayerController::OnZoom);
	EnhancedInput->BindAction(BackAction, ETriggerEvent::Started, this, &AConfiguratorPlayerController::OnBack);
}

void AConfiguratorPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	CreateDefaultInputAssets();
	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		InputSubsystem->AddMappingContext(MappingContext, 0);
	}

	// Курсор виден постоянно, клики идут и в UI, и в сцену (UI перехватывает только свои элементы).
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	if (HUDWidgetClass)
	{
		HUDWidget = CreateWidget<UConfiguratorHUDWidget>(this, HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport();
		}
	}
}

void AConfiguratorPlayerController::OnSelectStarted()
{
	bSelectPressed = true;
	bDragging = false;
	PressDragDistance = 0.f;
}

void AConfiguratorPlayerController::OnSelectCompleted()
{
	// Отпускание без заметного движения — это клик; иначе пользователь вращал камеру.
	const bool bWasClick = bSelectPressed && !bDragging;
	bSelectPressed = false;
	bDragging = false;

	if (bWasClick)
	{
		SelectApartmentUnderCursor();
	}
}

void AConfiguratorPlayerController::OnLook(const FInputActionValue& Value)
{
	const FVector2D Delta = Value.Get<FVector2D>();
	const bool bRightDown = IsInputKeyDown(EKeys::RightMouseButton);

	if (!bSelectPressed && !bRightDown)
	{
		return;
	}

	if (bSelectPressed && !bDragging && !bRightDown)
	{
		PressDragDistance += static_cast<float>(Delta.Size());
		if (PressDragDistance < DragThresholdPixels)
		{
			return;
		}
		bDragging = true;
	}

	if (AConfiguratorCameraPawn* CameraPawn = Cast<AConfiguratorCameraPawn>(GetPawn()))
	{
		CameraPawn->AddOrbitInput(static_cast<float>(Delta.X) * OrbitSensitivity, static_cast<float>(Delta.Y) * OrbitSensitivity);
	}
}

void AConfiguratorPlayerController::OnZoom(const FInputActionValue& Value)
{
	if (AConfiguratorCameraPawn* CameraPawn = Cast<AConfiguratorCameraPawn>(GetPawn()))
	{
		CameraPawn->AddZoomInput(Value.Get<float>());
	}
}

void AConfiguratorPlayerController::OnBack()
{
	if (UConfiguratorStateSubsystem* State = GetWorld()->GetSubsystem<UConfiguratorStateSubsystem>())
	{
		State->GoBack();
	}
}

void AConfiguratorPlayerController::SelectApartmentUnderCursor()
{
	// Одна трассировка на клик (а не каждый кадр): квартиры блокируют канал Visibility,
	// скрытые фильтром — игнорируют его, поэтому «проваливаемся» сквозь них.
	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		return;
	}

	const UApartmentMeshComponent* Apartment = Cast<UApartmentMeshComponent>(Hit.GetComponent());
	if (!Apartment)
	{
		return;
	}

	if (UConfiguratorStateSubsystem* State = GetWorld()->GetSubsystem<UConfiguratorStateSubsystem>())
	{
		State->ShowApartment(Apartment->GetApartmentId());
	}
}
