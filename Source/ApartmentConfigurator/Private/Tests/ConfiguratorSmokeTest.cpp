// Смоук-тест всего конфигуратора в запущенной игре (не в редакторе):
//   UnrealEditor.exe ApartmentConfigurator.uproject -game -ExecCmds="Automation RunTests Configurator.Game.Smoke"
// Проходит основной сценарий (общий вид -> этаж -> квартира -> фильтр -> «Назад» -> перезагрузка JSON), проверяет
// навигационное состояние и камеру, на каждом шаге сохраняет скриншот с UI в Saved/Screenshots/<платформа>/Smoke.
// Команды отдаются напрямую подсистеме состояния; реальный ввод мышью и клавиатурой проверяет Configurator.Game.Input.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ApartmentDataSubsystem.h"
#include "Core/ConfiguratorStateSubsystem.h"
#include "Tests/ConfiguratorTestUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfiguratorSmokeTest, "Configurator.Game.Smoke",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FConfiguratorSmokeTest::RunTest(const FString& Parameters)
{
	using namespace ConfiguratorTests;

	if (!FindGameWorld())
	{
		AddError(TEXT("No game world: run the test in the game (-game) or during PIE"));
		return false;
	}

	// Шаг 7 намеренно загружает синтаксически битый файл: сообщение об ошибке в логе — ожидаемый результат,
	// а не провал теста.
	AddExpectedError(TEXT("Failed to load configurator data"), EAutomationExpectedErrorFlags::Contains, 1);

	TSharedRef<float> FloorDistance = MakeShared<float>(0.f);

	// 1. Данные загружены асинхронно, здание построено, камера долетела до общего вида.
	WaitUntil(*this, TEXT("data and scene"), &IsSceneReady);
	WaitUntil(*this, TEXT("camera idle (genplan)"), &IsCameraIdle);
	WaitSeconds(*this, 1.0);
	Do([this]()
	{
		TestEqual(TEXT("Initial mode is Genplan"), GetState()->GetCurrentViewRef().Mode, EConfiguratorViewMode::Genplan);
		TestTrue(TEXT("Floors loaded"), GetData()->GetBuildingData().Floors.Num() > 0);
		RequestScreenshot(TEXT("01_Genplan"));
	});
	WaitSeconds(*this, 0.5);

	// 2. Вид этажа (индекс 3 — «4 этаж» в Building.json).
	Do([]() { GetState()->ShowFloor(3); });
	WaitUntil(*this, TEXT("camera idle (floor)"), &IsCameraIdle);
	WaitSeconds(*this, 0.5);
	Do([this, FloorDistance]()
	{
		TestEqual(TEXT("Floor mode"), GetState()->GetCurrentViewRef().Mode, EConfiguratorViewMode::Floor);
		TestEqual(TEXT("Floor index"), GetState()->GetCurrentViewRef().FloorIndex, 3);
		*FloorDistance = GetCameraDistance();
		RequestScreenshot(TEXT("02_Floor"));
	});
	WaitSeconds(*this, 0.5);

	// 3. Фокус на свободной квартире: карточка, контур, камера ближе, чем в виде этажа.
	Do([this]() { TestTrue(TEXT("ShowApartment(403)"), GetState()->ShowApartment(TEXT("403"))); });
	WaitUntil(*this, TEXT("camera idle (apartment)"), &IsCameraIdle);
	WaitSeconds(*this, 0.5);
	Do([this, FloorDistance]()
	{
		const FConfiguratorViewState& View = GetState()->GetCurrentViewRef();
		TestEqual(TEXT("Apartment mode"), View.Mode, EConfiguratorViewMode::Apartment);
		TestEqual(TEXT("Selected apartment"), View.ApartmentId, FString(TEXT("403")));
		TestTrue(TEXT("Camera moved closer than the floor view"), GetCameraDistance() < *FloorDistance);
		RequestScreenshot(TEXT("03_Apartment"));
	});
	WaitSeconds(*this, 0.5);

	// 4. Фильтр «Скрыть проданные»: проданную квартиру выбрать нельзя, текущая (свободная) остаётся.
	Do([this]()
	{
		GetState()->SetHideSold(true);
		TestFalse(TEXT("Sold apartment 402 is not selectable with the filter"), GetState()->ShowApartment(TEXT("402")));
		TestEqual(TEXT("Selection kept"), GetState()->GetCurrentViewRef().ApartmentId, FString(TEXT("403")));
	});
	WaitSeconds(*this, 0.5);
	Do([]() { RequestScreenshot(TEXT("04_HideSold")); });
	WaitSeconds(*this, 0.5);

	// 5. «Назад» дважды: квартира -> этаж -> общий вид.
	Do([this]()
	{
		TestTrue(TEXT("Back #1"), GetState()->GoBack());
		TestEqual(TEXT("Back to floor"), GetState()->GetCurrentViewRef().Mode, EConfiguratorViewMode::Floor);
		TestEqual(TEXT("Back to the same floor"), GetState()->GetCurrentViewRef().FloorIndex, 3);
		TestTrue(TEXT("Back #2"), GetState()->GoBack());
		TestEqual(TEXT("Back to genplan"), GetState()->GetCurrentViewRef().Mode, EConfiguratorViewMode::Genplan);
	});
	WaitUntil(*this, TEXT("camera idle (back)"), &IsCameraIdle);
	WaitSeconds(*this, 0.5);
	Do([]() { RequestScreenshot(TEXT("05_Back")); });
	WaitSeconds(*this, 0.5);
	Do([]() { GetState()->SetHideSold(false); });

	// 6. Перезагрузка данных «на лету». Файл с битыми полями загружается, и здание перестраивается;
	//    история навигации при этом сбрасывается (её снимки относятся к старым данным).
	Do([]() { GetData()->LoadFromFile(TEXT("Data/Building_Messy.json")); });
	WaitUntil(*this, TEXT("messy file loaded"), []()
	{
		return GetData()->GetLoadState() == EConfiguratorLoadState::Loaded && GetData()->GetBuildingData().Floors.Num() == 3;
	});
	WaitUntil(*this, TEXT("camera idle (reload)"), &IsCameraIdle);
	WaitSeconds(*this, 0.7);
	Do([this]()
	{
		TestTrue(TEXT("Messy file produced warnings"), GetData()->GetLastWarnings().Num() > 0);
		TestEqual(TEXT("View reset after reload"), GetState()->GetCurrentViewRef().Mode, EConfiguratorViewMode::Genplan);
		TestFalse(TEXT("History cleared after reload"), GetState()->CanGoBack());
		RequestScreenshot(TEXT("06_MessyFile"));
	});
	WaitSeconds(*this, 0.5);

	// 7. Синтаксически битый файл отклоняется целиком, прежние данные остаются в силе.
	Do([]() { GetData()->LoadFromFile(TEXT("Data/Building_Broken.json")); });
	WaitUntil(*this, TEXT("broken file rejected"), []() { return GetData()->GetLoadState() == EConfiguratorLoadState::Failed; });
	WaitSeconds(*this, 0.5);
	Do([this]()
	{
		TestTrue(TEXT("Previous data kept after a broken file"), GetData()->HasData() && GetData()->GetBuildingData().Floors.Num() == 3);
		TestFalse(TEXT("Error text is set"), GetData()->GetLastError().IsEmpty());
		RequestScreenshot(TEXT("07_BrokenFile"));
	});
	WaitSeconds(*this, 0.5);

	// 8. Возврат к основному файлу из настроек проекта.
	Do([]() { GetData()->RequestLoad(); });
	WaitUntil(*this, TEXT("main file reloaded"), []()
	{
		return GetData()->GetLoadState() == EConfiguratorLoadState::Loaded && GetData()->GetBuildingData().Floors.Num() == 7;
	});
	WaitUntil(*this, TEXT("camera idle (final)"), &IsCameraIdle);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
