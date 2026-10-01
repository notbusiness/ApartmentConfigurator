// Автотесты парсера: Session Frontend -> Automation -> "Configurator.Data".
// Проверяют главное требование к данным: некорректный JSON не роняет приложение,
// а битые поля заменяются значениями по умолчанию.

#include "ConfiguratorJsonParser.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfiguratorParserValidTest, "Configurator.Data.Parser.ValidFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FConfiguratorParserValidTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"({
		"building": "Test",
		"floors": [
			{ "number": 2, "name": "Second", "apartments": [
				{ "id": "201", "status": "Продано", "area": 50.5, "focusPoint": { "x": 1, "y": 2, "z": 3 } } ] },
			{ "number": 1, "apartments": [
				{ "id": 101, "status": "Свободно", "area": "42,5", "focusPoint": [10, 20, 30] } ] }
		]
	})");

	const FConfiguratorParseResult Result = FConfiguratorJsonParser::ParseString(Json);

	TestTrue(TEXT("Parsed"), Result.bSuccess);
	TestEqual(TEXT("Building name"), Result.Data.Name, FString(TEXT("Test")));
	if (!TestEqual(TEXT("Floor count"), Result.Data.Floors.Num(), 2))
	{
		return false;
	}

	// Этажи отсортированы по номеру.
	const FFloorData& First = Result.Data.Floors[0];
	TestEqual(TEXT("Sorted by number"), First.Number, 1);
	TestEqual(TEXT("Default floor name"), First.Name, FString(TEXT("Этаж 1")));
	if (TestEqual(TEXT("Apartments on floor 1"), First.Apartments.Num(), 1))
	{
		const FApartmentData& Apartment = First.Apartments[0];
		TestEqual(TEXT("Numeric id"), Apartment.Id, FString(TEXT("101")));
		TestEqual(TEXT("Status"), Apartment.Status, EApartmentStatus::Available);
		TestEqual(TEXT("Area from string with comma"), Apartment.Area, 42.5f);
		TestTrue(TEXT("Focus from array"), Apartment.bHasFocusPoint && Apartment.FocusPoint.Equals(FVector(10, 20, 30)));
	}

	const FFloorData& Second = Result.Data.Floors[1];
	if (TestEqual(TEXT("Apartments on floor 2"), Second.Apartments.Num(), 1))
	{
		TestEqual(TEXT("Sold status"), Second.Apartments[0].Status, EApartmentStatus::Sold);
		TestTrue(TEXT("Focus from object"), Second.Apartments[0].FocusPoint.Equals(FVector(1, 2, 3)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfiguratorParserBrokenFieldsTest, "Configurator.Data.Parser.BrokenFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FConfiguratorParserBrokenFieldsTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"({
		"floors": [
			{ "apartments": [
				{ "status": 5, "area": -3 },
				{ "id": "A", "status": "reserved", "area": null, "focusPoint": [1, 2] },
				"garbage",
				null,
				{ "id": "A", "status": "sold", "area": 1e400, "focusPoint": { "x": "abc", "y": 0, "z": 0 } }
			] },
			42,
			{ "number": "7", "apartments": "not an array" }
		]
	})");

	const FConfiguratorParseResult Result = FConfiguratorJsonParser::ParseString(Json);

	TestTrue(TEXT("Broken fields do not fail the whole file"), Result.bSuccess);
	TestTrue(TEXT("Warnings are reported"), Result.Warnings.Num() > 0);
	if (!TestEqual(TEXT("Valid floors kept"), Result.Data.Floors.Num(), 2))
	{
		return false;
	}

	const FFloorData& Floor = Result.Data.Floors[0];
	TestEqual(TEXT("Missing number falls back to index"), Floor.Number, 1);
	if (TestEqual(TEXT("Non-object apartments skipped"), Floor.Apartments.Num(), 3))
	{
		TestEqual(TEXT("Generated id"), Floor.Apartments[0].Id, FString(TEXT("1-1")));
		TestEqual(TEXT("Invalid status type"), Floor.Apartments[0].Status, EApartmentStatus::Unknown);
		TestEqual(TEXT("Negative area reset"), Floor.Apartments[0].Area, 0.f);
		TestFalse(TEXT("Short focus array rejected"), Floor.Apartments[1].bHasFocusPoint);
		TestEqual(TEXT("Unknown status text"), Floor.Apartments[1].Status, EApartmentStatus::Unknown);
		TestEqual(TEXT("Duplicate id renamed"), Floor.Apartments[2].Id, FString(TEXT("A (2)")));
		TestFalse(TEXT("Non-numeric focus rejected"), Floor.Apartments[2].bHasFocusPoint);
	}

	TestEqual(TEXT("Numeric string floor number"), Result.Data.Floors[1].Number, 7);
	TestEqual(TEXT("Invalid apartments array"), Result.Data.Floors[1].Apartments.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfiguratorParserInvalidFileTest, "Configurator.Data.Parser.InvalidFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FConfiguratorParserInvalidFileTest::RunTest(const FString& Parameters)
{
	// Ни один из вариантов не должен приводить к падению — только к bSuccess == false с описанием ошибки.
	const TCHAR* Inputs[] = {
		TEXT(""),
		TEXT("   "),
		TEXT("{ \"floors\": [ { \"number\": 1, "),
		TEXT("not json at all"),
		TEXT("{ \"building\": \"No floors\" }"),
		TEXT("{ \"floors\": [] }"),
		TEXT("{ \"floors\": [1, \"two\", null] }"),
		TEXT("123")
	};

	for (const TCHAR* Input : Inputs)
	{
		const FConfiguratorParseResult Result = FConfiguratorJsonParser::ParseString(Input);
		TestFalse(FString::Printf(TEXT("Rejected: '%s'"), Input), Result.bSuccess);
		TestFalse(FString::Printf(TEXT("Has error text: '%s'"), Input), Result.Error.IsEmpty());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
