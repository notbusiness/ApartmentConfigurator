// Тест реального ввода в запущенной игре:
//   UnrealEditor.exe ApartmentConfigurator.uproject -game -ExecCmds="Automation RunTests Configurator.Game.Input"
//
// События мыши и клавиатуры подаются в Slate (FSlateApplication::Process*Event) — тем же путём, что и от
// настоящих устройств. Поэтому проверяется вся цепочка: попадание по виджетам HUD и то, что пустые области
// HUD пропускают клик в сцену; вьюпорт; Enhanced Input; контроллер; трассировка; подсистема состояния.
// Во время теста не трогайте мышь: курсор тоже переставляется.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Core/ConfiguratorSceneProvider.h"
#include "Core/ConfiguratorStateSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Tests/ConfiguratorTestUtils.h"
#include "UI/ApartmentCardWidget.h"
#include "UI/ConfiguratorHUDWidget.h"
#include "UI/ConfiguratorListButton.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "Widgets/SViewport.h"

namespace ConfiguratorInput
{
	using namespace ConfiguratorTests;

	// ---------------------------------------------------------------- Поиск виджетов

	UConfiguratorHUDWidget* FindHUD()
	{
		const UWorld* World = FindGameWorld();
		for (TObjectIterator<UConfiguratorHUDWidget> It; It; ++It)
		{
			if (It->GetWorld() == World)
			{
				return *It;
			}
		}
		return nullptr;
	}

	template <typename TWidget>
	TWidget* FindWidget(UUserWidget* Root, TFunctionRef<bool(TWidget*)> Predicate)
	{
		TWidget* Found = nullptr;
		if (Root && Root->WidgetTree)
		{
			Root->WidgetTree->ForEachWidget([&Found, &Predicate](UWidget* Widget)
			{
				TWidget* Typed = Cast<TWidget>(Widget);
				if (!Found && Typed && Predicate(Typed))
				{
					Found = Typed;
				}
			});
		}
		return Found;
	}

	/** Кнопка, у которой подпись (первый дочерний UTextBlock) содержит Label. */
	UButton* FindButtonByLabel(UUserWidget* Root, const FString& Label)
	{
		return FindWidget<UButton>(Root, [&Label](UButton* Button)
		{
			const UTextBlock* Text = Cast<UTextBlock>(Button->GetChildAt(0));
			return Text && Text->GetText().ToString().Contains(Label);
		});
	}

	TOptional<FVector2D> WidgetCenter(const UWidget* Widget)
	{
		if (!Widget)
		{
			return {};
		}
		return FVector2D(Widget->GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D(0.5, 0.5)));
	}

	// ---------------------------------------------------------------- Координаты

	/** Точка мира -> абсолютные (экранные) координаты Slate внутри окна игры. */
	TOptional<FVector2D> WorldToAbsolute(const FVector& WorldPoint)
	{
		APlayerController* Controller = GetPlayerController();
		UGameViewportClient* ViewportClient = GEngine->GameViewport;
		if (!Controller || !ViewportClient || !ViewportClient->Viewport)
		{
			return {};
		}

		FVector2D Pixel;
		if (!Controller->ProjectWorldLocationToScreen(WorldPoint, Pixel))
		{
			return {};
		}

		const TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget();
		if (!ViewportWidget.IsValid())
		{
			return {};
		}

		// Пиксели вьюпорта -> локальные единицы Slate (с учётом DPI) -> абсолютные координаты.
		const FGeometry& Geometry = ViewportWidget->GetCachedGeometry();
		const FIntPoint PixelSize = ViewportClient->Viewport->GetSizeXY();
		const FVector2D LocalSize(Geometry.GetLocalSize());
		const FVector2D Local = Pixel * LocalSize / FVector2D(FMath::Max(PixelSize.X, 1), FMath::Max(PixelSize.Y, 1));
		return FVector2D(Geometry.LocalToAbsolute(Local));
	}

	/** Центр вьюпорта — там всегда сцена, а не панели HUD. */
	TOptional<FVector2D> ViewportCenter()
	{
		UGameViewportClient* ViewportClient = GEngine->GameViewport;
		const TSharedPtr<SViewport> ViewportWidget = ViewportClient ? ViewportClient->GetGameViewportWidget() : nullptr;
		if (!ViewportWidget.IsValid())
		{
			return {};
		}
		const FGeometry& Geometry = ViewportWidget->GetCachedGeometry();
		return FVector2D(Geometry.LocalToAbsolute(FVector2D(Geometry.GetLocalSize()) * 0.5));
	}

	// ---------------------------------------------------------------- События Slate

	FPointerEvent MakePointer(const FVector2D& Position, const FVector2D& LastPosition, const TSet<FKey>& Pressed, const FKey& Effecting, float WheelDelta = 0.f)
	{
		return FPointerEvent(0, FSlateApplicationBase::CursorPointerIndex, Position, LastPosition, Pressed, Effecting, WheelDelta, FModifierKeysState());
	}

	void MoveMouse(const FVector2D& From, const FVector2D& To, const TSet<FKey>& Pressed = TSet<FKey>())
	{
		FSlateApplication& Slate = FSlateApplication::Get();
		Slate.SetCursorPos(To);
		Slate.ProcessMouseMoveEvent(MakePointer(To, From, Pressed, EKeys::Invalid));
	}

	void PressMouse(const FVector2D& At, const FKey& Button)
	{
		FSlateApplication::Get().ProcessMouseButtonDownEvent(nullptr, MakePointer(At, At, TSet<FKey>({ Button }), Button));
	}

	void ReleaseMouse(const FVector2D& At, const FKey& Button)
	{
		FSlateApplication::Get().ProcessMouseButtonUpEvent(MakePointer(At, At, TSet<FKey>(), Button));
	}

	void ScrollWheel(const FVector2D& At, float Delta)
	{
		FSlateApplication::Get().ProcessMouseWheelOrGestureEvent(MakePointer(At, At, TSet<FKey>(), EKeys::Invalid, Delta), nullptr);
	}

	void PressKey(const FKey& Key)
	{
		FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
	}

	void ReleaseKey(const FKey& Key)
	{
		FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
	}

	/** Диагностика: какой виджет Slate находится под точкой и держит ли кто-то захват мыши. */
	void LogUnderCursor(const TCHAR* What, const FVector2D& Position)
	{
		FSlateApplication& Slate = FSlateApplication::Get();
		const FWidgetPath Path = Slate.LocateWindowUnderMouse(Position, Slate.GetInteractiveTopLevelWindows(), true);
		const FString Widget = Path.IsValid() ? Path.GetLastWidget()->GetTypeAsString() + TEXT(" ") + Path.GetLastWidget()->GetReadableLocation() : FString(TEXT("none"));
		UE_LOG(LogTemp, Display, TEXT("[InputTest] %s at (%.0f, %.0f): under cursor = %s, mouse captor = %d"),
			What, Position.X, Position.Y, *Widget, Slate.HasAnyMouseCaptor() ? 1 : 0);
	}

	/**
	 * Латентный клик левой кнопкой: навести курсор, нажать, отпустить — в разных кадрах,
	 * как у живого пользователя. Where вычисляется в момент клика (виджеты уже разложены).
	 */
	void Click(FAutomationTestBase& Test, const TCHAR* What, TFunction<TOptional<FVector2D>()> Where)
	{
		FAutomationTestBase* TestPtr = &Test;
		TSharedRef<FVector2D> Position = MakeShared<FVector2D>(FVector2D::ZeroVector);
		Do([TestPtr, What, Where, Position]()
		{
			const TOptional<FVector2D> Target = Where();
			if (!Target.IsSet())
			{
				TestPtr->AddError(FString::Printf(TEXT("Click target not found: %s"), What));
				return;
			}
			*Position = Target.GetValue();
			MoveMouse(*Position, *Position);
			LogUnderCursor(What, *Position);
		});
		WaitSeconds(Test, 0.15);
		Do([Position]() { PressMouse(*Position, EKeys::LeftMouseButton); });
		WaitSeconds(Test, 0.15);
		Do([Position]()
		{
			ReleaseMouse(*Position, EKeys::LeftMouseButton);
			LogUnderCursor(TEXT("after release"), *Position);
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfiguratorInputTest, "Configurator.Game.Input",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FConfiguratorInputTest::RunTest(const FString& Parameters)
{
	using namespace ConfiguratorInput;

	if (!FindGameWorld())
	{
		AddError(TEXT("No game world: run the test in the game (-game) or during PIE"));
		return false;
	}

	// «Забронировать» должно оставить строку в логе — это требование задания.
	AddExpectedMessage(TEXT("Booking requested: apartment '404'"), ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);

	WaitUntil(*this, TEXT("data and scene"), &IsSceneReady);
	WaitUntil(*this, TEXT("camera idle (start)"), &IsCameraIdle);
	WaitSeconds(*this, 1.0);

	// 1. Кнопка этажа в панели (создана по JSON) -> вид этажа.
	Click(*this, TEXT("floor button '4'"), []() -> TOptional<FVector2D>
	{
		return WidgetCenter(FindWidget<UConfiguratorListButton>(FindHUD(), [](UConfiguratorListButton* Button)
		{
			return Button->GetIntPayload() == 3 && Button->GetStringPayload().IsEmpty();
		}));
	});
	WaitUntil(*this, TEXT("floor view after clicking the floor button"), []()
	{
		const FConfiguratorViewState& View = GetState()->GetCurrentViewRef();
		return View.Mode == EConfiguratorViewMode::Floor && View.FloorIndex == 3;
	});
	WaitUntil(*this, TEXT("camera idle (floor)"), &IsCameraIdle);
	WaitSeconds(*this, 0.3);

	// 2. Клик мышью по квартире на фасаде -> Apartment Focus + карточка.
	Click(*this, TEXT("apartment 404 on the facade"), []() -> TOptional<FVector2D>
	{
		const IConfiguratorSceneProvider* Scene = GetState()->GetSceneProvider();
		FVector Focus;
		FBox Bounds(ForceInit);
		if (!Scene || !Scene->GetApartmentFocus(TEXT("404"), Focus, Bounds))
		{
			return {};
		}
		// Точка фокуса стоит перед фасадом на середине высоты этажа: луч из камеры попадает в фасад
		// именно этой квартиры (центр объёма лежит в глубине здания, и луч сверху вошёл бы этажом выше).
		return WorldToAbsolute(Focus);
	});
	WaitUntil(*this, TEXT("apartment 404 selected by a click in 3D"), []()
	{
		const FConfiguratorViewState& View = GetState()->GetCurrentViewRef();
		return View.Mode == EConfiguratorViewMode::Apartment && View.ApartmentId == TEXT("404");
	});
	WaitUntil(*this, TEXT("camera idle (apartment)"), &IsCameraIdle);
	WaitSeconds(*this, 0.5);
	Do([]() { RequestScreenshot(TEXT("08_InputClick")); });
	WaitSeconds(*this, 0.3);

	// 3. «Забронировать» в карточке -> строка в логе (проверяется AddExpectedMessage).
	Click(*this, TEXT("booking button"), []() -> TOptional<FVector2D>
	{
		UApartmentCardWidget* Card = FindWidget<UApartmentCardWidget>(FindHUD(), [](UApartmentCardWidget*) { return true; });
		return WidgetCenter(FindButtonByLabel(Card, TEXT("Забронировать")));
	});
	WaitSeconds(*this, 0.3);

	// 4. Колесо мыши -> зум (камера приближается плавно).
	TSharedRef<float> DistanceBefore = MakeShared<float>(0.f);
	Do([DistanceBefore]()
	{
		*DistanceBefore = GetCameraDistance();
		if (const TOptional<FVector2D> Center = ViewportCenter())
		{
			MoveMouse(*Center, *Center);
			for (int32 Notch = 0; Notch < 3; ++Notch)
			{
				ScrollWheel(*Center, 1.f);
			}
		}
	});
	WaitSeconds(*this, 1.0);
	Do([this, DistanceBefore]()
	{
		TestTrue(TEXT("Mouse wheel zooms in"), GetCameraDistance() < *DistanceBefore * 0.9f);
	});

	// 5. Перетаскивание с зажатой ПКМ -> вращение вокруг точки фокуса.
	TSharedRef<float> YawBefore = MakeShared<float>(0.f);
	TSharedRef<FVector2D> DragPosition = MakeShared<FVector2D>(FVector2D::ZeroVector);
	Do([YawBefore, DragPosition]()
	{
		*YawBefore = GetCameraYaw();
		*DragPosition = ViewportCenter().Get(FVector2D::ZeroVector);
		MoveMouse(*DragPosition, *DragPosition);
		PressMouse(*DragPosition, EKeys::RightMouseButton);
	});
	for (int32 Step = 0; Step < 8; ++Step)
	{
		WaitSeconds(*this, 0.05);
		Do([DragPosition]()
		{
			const FVector2D Next = *DragPosition + FVector2D(25.0, 0.0);
			MoveMouse(*DragPosition, Next, TSet<FKey>({ EKeys::RightMouseButton }));
			*DragPosition = Next;
			UE_LOG(LogTemp, Display, TEXT("[InputTest] drag step: yaw %.1f, mouse captor = %d"), GetCameraYaw(), FSlateApplication::Get().HasAnyMouseCaptor() ? 1 : 0);
		});
	}
	WaitSeconds(*this, 0.1);
	Do([DragPosition]() { ReleaseMouse(*DragPosition, EKeys::RightMouseButton); });
	WaitSeconds(*this, 0.2);
	Do([this, YawBefore]()
	{
		const float Rotated = FMath::Abs(FMath::FindDeltaAngleDegrees(*YawBefore, GetCameraYaw()));
		TestTrue(FString::Printf(TEXT("Right mouse drag rotates the camera (%.1f deg)"), Rotated), Rotated > 30.f && Rotated < 120.f);
	});

	// 6. Чекбокс «Скрыть проданные»: включить и выключить.
	auto CheckBoxCenter = []() -> TOptional<FVector2D>
	{
		return WidgetCenter(FindWidget<UCheckBox>(FindHUD(), [](UCheckBox*) { return true; }));
	};
	Click(*this, TEXT("hide sold checkbox (on)"), CheckBoxCenter);
	WaitUntil(*this, TEXT("filter enabled by the checkbox"), []() { return GetState()->IsHideSold(); });
	Click(*this, TEXT("hide sold checkbox (off)"), CheckBoxCenter);
	WaitUntil(*this, TEXT("filter disabled by the checkbox"), []() { return !GetState()->IsHideSold(); });

	// 7. Backspace -> назад (квартира -> этаж).
	Do([]() { PressKey(EKeys::BackSpace); });
	WaitSeconds(*this, 0.15);
	Do([]() { ReleaseKey(EKeys::BackSpace); });
	WaitUntil(*this, TEXT("back to the floor view by Backspace"), []()
	{
		return GetState()->GetCurrentViewRef().Mode == EConfiguratorViewMode::Floor;
	});
	WaitUntil(*this, TEXT("camera idle (back)"), &IsCameraIdle);

	// 8. Кнопка «Назад» в панели -> общий вид.
	Click(*this, TEXT("back button"), []() -> TOptional<FVector2D>
	{
		return WidgetCenter(FindButtonByLabel(FindHUD(), TEXT("Назад")));
	});
	WaitUntil(*this, TEXT("genplan after the back button"), []()
	{
		return GetState()->GetCurrentViewRef().Mode == EConfiguratorViewMode::Genplan;
	});
	WaitUntil(*this, TEXT("camera idle (end)"), &IsCameraIdle);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
