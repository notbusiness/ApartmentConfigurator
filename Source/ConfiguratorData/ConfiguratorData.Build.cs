using UnrealBuildTool;

// Модуль данных: типы, парсер JSON и подсистема загрузки.
// Намеренно не зависит от сцены, камеры и UI — его можно переиспользовать
// (например, в редакторных утилитах) и тестировать изолированно.
public class ConfiguratorData : ModuleRules
{
	public ConfiguratorData(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json"
		});
	}
}
