#include "ConfiguratorJsonParser.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ConfiguratorJson
{
	/** Не даём битому файлу завалить лог тысячами строк. */
	static constexpr int32 MaxStoredWarnings = 100;

	/** Разумный предел координат (±10 км в см): защищает камеру от улёта в бесконечность. */
	static constexpr double MaxCoordinateAbs = 1000000.0;

	static constexpr double MaxArea = 10000.0;

	/** Собирает предупреждения и считает те, что не поместились в лимит. */
	struct FWarningSink
	{
		TArray<FString>& Warnings;
		int32 Dropped = 0;

		explicit FWarningSink(TArray<FString>& InWarnings) : Warnings(InWarnings) {}

		void Add(FString&& Message)
		{
			if (Warnings.Num() < MaxStoredWarnings)
			{
				Warnings.Add(MoveTemp(Message));
			}
			else
			{
				++Dropped;
			}
		}

		void Finalize()
		{
			if (Dropped > 0)
			{
				Warnings.Add(FString::Printf(TEXT("... and %d more warnings"), Dropped));
			}
		}
	};

	/**
	 * Ищет поле по нескольким допустимым именам (алиасам).
	 * Используется публичный TryGetField, а не прямой доступ к FJsonObject::Values:
	 * тип ключей этой карты меняется между версиями движка (5.7 -> 5.8).
	 */
	static TSharedPtr<FJsonValue> FindField(const TSharedPtr<FJsonObject>& Object, std::initializer_list<const TCHAR*> Names)
	{
		if (!Object.IsValid())
		{
			return nullptr;
		}

		for (const TCHAR* Name : Names)
		{
			TSharedPtr<FJsonValue> Found = Object->TryGetField(Name);

			// Явный null в JSON равнозначен отсутствию поля.
			if (Found.IsValid() && Found->Type != EJson::Null)
			{
				return Found;
			}
		}
		return nullptr;
	}

	/** Число из JSON-числа или из строки ("42.5", "42,5"). NaN/Inf отбрасываются. */
	static bool ReadNumber(const TSharedPtr<FJsonValue>& Value, double& OutNumber)
	{
		if (!Value.IsValid())
		{
			return false;
		}

		double Parsed = 0.0;
		bool bParsed = false;

		if (Value->Type == EJson::Number)
		{
			bParsed = Value->TryGetNumber(Parsed);
		}
		else if (Value->Type == EJson::String)
		{
			FString Text;
			Value->TryGetString(Text);
			Text.TrimStartAndEndInline();
			Text.ReplaceInline(TEXT(","), TEXT("."));
			bParsed = !Text.IsEmpty() && LexTryParseString(Parsed, *Text);
		}

		if (bParsed && FMath::IsFinite(Parsed))
		{
			OutNumber = Parsed;
			return true;
		}
		return false;
	}

	/** Непустая строка из JSON-строки или числа (ID вида 101 без кавычек тоже допустим). */
	static bool ReadIdString(const TSharedPtr<FJsonValue>& Value, FString& OutText)
	{
		if (!Value.IsValid())
		{
			return false;
		}

		if (Value->Type == EJson::String)
		{
			FString Text;
			Value->TryGetString(Text);
			Text.TrimStartAndEndInline();
			if (!Text.IsEmpty())
			{
				OutText = MoveTemp(Text);
				return true;
			}
			return false;
		}

		double Number = 0.0;
		if (Value->Type == EJson::Number && ReadNumber(Value, Number))
		{
			// 101.0 -> "101", 12.5 -> "12.5"
			OutText = (FMath::Frac(Number) == 0.0 && FMath::Abs(Number) < 1e15)
				? FString::Printf(TEXT("%lld"), static_cast<int64>(Number))
				: FString::SanitizeFloat(Number);
			return true;
		}
		return false;
	}

	/** Точка фокуса: объект {x, y, z} или массив [x, y, z]. Требуются все три координаты. */
	static bool ReadVector(const TSharedPtr<FJsonValue>& Value, FVector& OutVector)
	{
		if (!Value.IsValid())
		{
			return false;
		}

		double Components[3] = { 0.0, 0.0, 0.0 };

		const TSharedPtr<FJsonObject>* Object = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;

		if (Value->TryGetObject(Object) && Object && Object->IsValid())
		{
			const TCHAR* Axes[3] = { TEXT("x"), TEXT("y"), TEXT("z") };
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				if (!ReadNumber(FindField(*Object, { Axes[Axis] }), Components[Axis]))
				{
					return false;
				}
			}
		}
		else if (Value->TryGetArray(Array) && Array && Array->Num() == 3)
		{
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				if (!ReadNumber((*Array)[Axis], Components[Axis]))
				{
					return false;
				}
			}
		}
		else
		{
			return false;
		}

		// Координата за пределами разумного диапазона — почти наверняка ошибка данных
		// (например, миллиметры вместо сантиметров). Отбрасываем точку целиком, сцена подставит центр квартиры.
		for (const double Component : Components)
		{
			if (FMath::Abs(Component) > MaxCoordinateAbs)
			{
				return false;
			}
		}

		OutVector = FVector(Components[0], Components[1], Components[2]);
		return true;
	}

	/** Разбор одной квартиры. Всегда возвращает валидную запись: отсутствующие поля заменяются значениями по умолчанию. */
	static FApartmentData ParseApartment(const TSharedPtr<FJsonObject>& Object, const FString& Context, const FString& FallbackId, FWarningSink& Sink)
	{
		FApartmentData Apartment;

		// --- ID ---
		if (!ReadIdString(FindField(Object, { TEXT("id"), TEXT("apartmentId"), TEXT("number") }), Apartment.Id))
		{
			Apartment.Id = FallbackId;
			Sink.Add(FString::Printf(TEXT("%s: missing or invalid 'id', generated '%s'"), *Context, *FallbackId));
		}

		// --- Статус ---
		FString StatusText;
		const TSharedPtr<FJsonValue> StatusValue = FindField(Object, { TEXT("status"), TEXT("state") });
		if (StatusValue.IsValid() && StatusValue->Type == EJson::String && StatusValue->TryGetString(StatusText))
		{
			bool bRecognized = false;
			Apartment.Status = FConfiguratorJsonParser::ParseStatus(StatusText, bRecognized);
			if (!bRecognized)
			{
				Sink.Add(FString::Printf(TEXT("%s (id '%s'): unknown status '%s'"), *Context, *Apartment.Id, *StatusText));
			}
		}
		else
		{
			Apartment.Status = EApartmentStatus::Unknown;
			Sink.Add(FString::Printf(TEXT("%s (id '%s'): missing or invalid 'status'"), *Context, *Apartment.Id));
		}

		// --- Площадь ---
		double Area = 0.0;
		if (ReadNumber(FindField(Object, { TEXT("area"), TEXT("square") }), Area) && Area > 0.0)
		{
			Apartment.Area = static_cast<float>(FMath::Min(Area, MaxArea));
		}
		else
		{
			Apartment.Area = 0.f;
			Sink.Add(FString::Printf(TEXT("%s (id '%s'): missing or invalid 'area'"), *Context, *Apartment.Id));
		}

		// --- Точка фокуса камеры ---
		Apartment.bHasFocusPoint = ReadVector(FindField(Object, { TEXT("focusPoint"), TEXT("focus"), TEXT("cameraFocus") }), Apartment.FocusPoint);
		if (!Apartment.bHasFocusPoint)
		{
			Sink.Add(FString::Printf(TEXT("%s (id '%s'): missing or invalid 'focusPoint', scene center will be used"), *Context, *Apartment.Id));
		}

		return Apartment;
	}

	/** Делает ID уникальным в пределах здания: "101" -> "101 (2)". */
	static FString MakeUniqueId(const FString& Id, TSet<FString>& UsedIds)
	{
		if (!UsedIds.Contains(Id))
		{
			UsedIds.Add(Id);
			return Id;
		}

		for (int32 Suffix = 2; ; ++Suffix)
		{
			FString Candidate = FString::Printf(TEXT("%s (%d)"), *Id, Suffix);
			if (!UsedIds.Contains(Candidate))
			{
				UsedIds.Add(Candidate);
				return Candidate;
			}
		}
	}
}

EApartmentStatus FConfiguratorJsonParser::ParseStatus(const FString& StatusText, bool& bOutRecognized)
{
	const FString Normalized = StatusText.TrimStartAndEnd();

	// Кириллица сравнивается по списку вариантов написания: сравнение без учёта регистра
	// в FString надёжно работает только для ASCII.
	static const TCHAR* const SoldValues[] = {
		TEXT("Продано"), TEXT("продано"), TEXT("ПРОДАНО"), TEXT("Продана"), TEXT("продана"), TEXT("sold") };
	static const TCHAR* const AvailableValues[] = {
		TEXT("Свободно"), TEXT("свободно"), TEXT("СВОБОДНО"), TEXT("Свободна"), TEXT("свободна"),
		TEXT("available"), TEXT("free") };

	for (const TCHAR* Value : SoldValues)
	{
		if (Normalized.Equals(Value, ESearchCase::IgnoreCase))
		{
			bOutRecognized = true;
			return EApartmentStatus::Sold;
		}
	}
	for (const TCHAR* Value : AvailableValues)
	{
		if (Normalized.Equals(Value, ESearchCase::IgnoreCase))
		{
			bOutRecognized = true;
			return EApartmentStatus::Available;
		}
	}

	bOutRecognized = false;
	return EApartmentStatus::Unknown;
}

FConfiguratorParseResult FConfiguratorJsonParser::ParseString(const FString& JsonText)
{
	using namespace ConfiguratorJson;

	FConfiguratorParseResult Result;
	FWarningSink Sink(Result.Warnings);

	if (JsonText.TrimStartAndEnd().IsEmpty())
	{
		Result.Error = TEXT("File is empty");
		return Result;
	}

	// 1. Синтаксический разбор. Ошибка синтаксиса — единственный случай, когда файл отклоняется целиком.
	TSharedPtr<FJsonValue> RootValue;
	const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootValue) || !RootValue.IsValid())
	{
		Result.Error = FString::Printf(TEXT("Invalid JSON: %s"), *Reader->GetErrorMessage());
		return Result;
	}

	// 2. Корень: объект с полем "floors" или сразу массив этажей.
	const TArray<TSharedPtr<FJsonValue>>* FloorValues = nullptr;
	const TSharedPtr<FJsonObject>* RootObject = nullptr;

	if (RootValue->TryGetObject(RootObject) && RootObject && RootObject->IsValid())
	{
		FString BuildingName;
		const TSharedPtr<FJsonValue> NameValue = FindField(*RootObject, { TEXT("building"), TEXT("name") });
		if (NameValue.IsValid() && NameValue->Type == EJson::String && NameValue->TryGetString(BuildingName))
		{
			Result.Data.Name = BuildingName.TrimStartAndEnd();
		}

		const TSharedPtr<FJsonValue> FloorsValue = FindField(*RootObject, { TEXT("floors"), TEXT("levels") });
		if (!FloorsValue.IsValid() || !FloorsValue->TryGetArray(FloorValues))
		{
			Result.Error = TEXT("Root object has no 'floors' array");
			return Result;
		}
	}
	else if (!RootValue->TryGetArray(FloorValues))
	{
		Result.Error = TEXT("Root must be an object with 'floors' or an array of floors");
		return Result;
	}

	if (FloorValues->Num() > MaxFloors)
	{
		Sink.Add(FString::Printf(TEXT("Too many floors (%d), only the first %d are used"), FloorValues->Num(), MaxFloors));
	}

	// 3. Этажи и квартиры. Любая некорректная запись пропускается или исправляется, но не прерывает разбор.
	TSet<FString> UsedApartmentIds;
	TSet<int32> UsedFloorNumbers;
	const int32 FloorCount = FMath::Min(FloorValues->Num(), MaxFloors);

	for (int32 FloorIndex = 0; FloorIndex < FloorCount; ++FloorIndex)
	{
		const TSharedPtr<FJsonValue>& FloorValue = (*FloorValues)[FloorIndex];
		const TSharedPtr<FJsonObject>* FloorObject = nullptr;
		if (!FloorValue.IsValid() || !FloorValue->TryGetObject(FloorObject) || !FloorObject || !FloorObject->IsValid())
		{
			Sink.Add(FString::Printf(TEXT("floors[%d]: not an object, skipped"), FloorIndex));
			continue;
		}

		FFloorData Floor;

		double Number = 0.0;
		if (ReadNumber(FindField(*FloorObject, { TEXT("number"), TEXT("floor"), TEXT("level") }), Number))
		{
			Floor.Number = FMath::Clamp(FMath::RoundToInt32(Number), -99, 999);
		}
		else
		{
			Floor.Number = FloorIndex + 1;
			Sink.Add(FString::Printf(TEXT("floors[%d]: missing or invalid 'number', using %d"), FloorIndex, Floor.Number));
		}

		if (UsedFloorNumbers.Contains(Floor.Number))
		{
			Sink.Add(FString::Printf(TEXT("floors[%d]: duplicate floor number %d"), FloorIndex, Floor.Number));
		}
		UsedFloorNumbers.Add(Floor.Number);

		const TSharedPtr<FJsonValue> FloorNameValue = FindField(*FloorObject, { TEXT("name"), TEXT("title") });
		if (!FloorNameValue.IsValid() || FloorNameValue->Type != EJson::String || !FloorNameValue->TryGetString(Floor.Name)
			|| Floor.Name.TrimStartAndEnd().IsEmpty())
		{
			Floor.Name = FString::Printf(TEXT("Этаж %d"), Floor.Number);
		}

		const TArray<TSharedPtr<FJsonValue>>* ApartmentValues = nullptr;
		const TSharedPtr<FJsonValue> ApartmentsField = FindField(*FloorObject, { TEXT("apartments"), TEXT("flats") });
		if (!ApartmentsField.IsValid() || !ApartmentsField->TryGetArray(ApartmentValues))
		{
			Sink.Add(FString::Printf(TEXT("Floor %d: missing or invalid 'apartments', floor has no apartments"), Floor.Number));
		}
		else
		{
			if (ApartmentValues->Num() > MaxApartmentsPerFloor)
			{
				Sink.Add(FString::Printf(TEXT("Floor %d: too many apartments (%d), only the first %d are used"),
					Floor.Number, ApartmentValues->Num(), MaxApartmentsPerFloor));
			}

			const int32 ApartmentCount = FMath::Min(ApartmentValues->Num(), MaxApartmentsPerFloor);
			for (int32 ApartmentIndex = 0; ApartmentIndex < ApartmentCount; ++ApartmentIndex)
			{
				const FString Context = FString::Printf(TEXT("Floor %d, apartments[%d]"), Floor.Number, ApartmentIndex);

				const TSharedPtr<FJsonValue>& ApartmentValue = (*ApartmentValues)[ApartmentIndex];
				const TSharedPtr<FJsonObject>* ApartmentObject = nullptr;
				if (!ApartmentValue.IsValid() || !ApartmentValue->TryGetObject(ApartmentObject) || !ApartmentObject || !ApartmentObject->IsValid())
				{
					Sink.Add(FString::Printf(TEXT("%s: not an object, skipped"), *Context));
					continue;
				}

				const FString FallbackId = FString::Printf(TEXT("%d-%d"), Floor.Number, ApartmentIndex + 1);
				FApartmentData Apartment = ParseApartment(*ApartmentObject, Context, FallbackId, Sink);

				const FString UniqueId = MakeUniqueId(Apartment.Id, UsedApartmentIds);
				if (UniqueId != Apartment.Id)
				{
					Sink.Add(FString::Printf(TEXT("%s: duplicate id '%s' renamed to '%s'"), *Context, *Apartment.Id, *UniqueId));
					Apartment.Id = UniqueId;
				}

				Floor.Apartments.Add(MoveTemp(Apartment));
			}
		}

		Result.Data.Floors.Add(MoveTemp(Floor));
	}

	if (Result.Data.Floors.IsEmpty())
	{
		Result.Error = TEXT("File contains no valid floors");
		Sink.Finalize();
		return Result;
	}

	// 4. Этажи снизу вверх по номеру. Стабильная сортировка сохраняет порядок этажей с одинаковым номером.
	Result.Data.Floors.StableSort([](const FFloorData& A, const FFloorData& B) { return A.Number < B.Number; });

	Sink.Finalize();
	Result.bSuccess = true;
	return Result;
}
