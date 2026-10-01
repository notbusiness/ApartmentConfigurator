#pragma once

#include "CoreMinimal.h"
#include "ConfiguratorDataTypes.h"

/** Результат разбора: данные + список предупреждений о пропущенных/исправленных полях. */
struct CONFIGURATORDATA_API FConfiguratorParseResult
{
	/** false — файл не удалось использовать целиком (не JSON, нет массива этажей и т.п.). */
	bool bSuccess = false;

	FBuildingData Data;

	/** Некритичные проблемы: поле отсутствует, неверный тип, дубликат ID... Данные при этом валидны. */
	TArray<FString> Warnings;

	/** Причина неудачи при bSuccess == false. */
	FString Error;
};

/**
 * Толерантный парсер конфигурации здания.
 *
 * Принципы:
 *  - никогда не падает и не использует check() на входных данных: любой некорректный
 *    фрагмент либо заменяется значением по умолчанию, либо пропускается с предупреждением;
 *  - не создаёт UObject и не трогает глобальное состояние, поэтому вызывается из фонового потока;
 *  - все лимиты (кол-во этажей/квартир) ограничены, чтобы битый или огромный файл
 *    не повесил приложение.
 *
 * Поддерживаемый формат (см. Content/Data/Building.json):
 * {
 *   "building": "Название",
 *   "floors": [
 *     { "number": 1, "name": "1 этаж", "apartments": [
 *         { "id": "101", "status": "Свободно", "area": 42.5,
 *           "focusPoint": { "x": 0, "y": -500, "z": 160 } } ] } ]
 * }
 * Корнем может быть и сам массив этажей. focusPoint принимается также как массив [x, y, z].
 */
class CONFIGURATORDATA_API FConfiguratorJsonParser
{
public:
	static constexpr int32 MaxFloors = 200;
	static constexpr int32 MaxApartmentsPerFloor = 100;

	/** Разбирает JSON-строку. Потокобезопасен. */
	static FConfiguratorParseResult ParseString(const FString& JsonText);

	/** Преобразует строку статуса ("Продано", "Свободно", "sold", "available"...) в enum. */
	static EApartmentStatus ParseStatus(const FString& StatusText, bool& bOutRecognized);
};
