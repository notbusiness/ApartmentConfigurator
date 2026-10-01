using UnrealBuildTool;

// Игровой модуль: навигация (состояние), камера, сцена, ввод и UI.
// Данные получает только через публичный интерфейс модуля ConfiguratorData.
public class ApartmentConfigurator : ModuleRules
{
	public ApartmentConfigurator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"ConfiguratorData"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore"
		});
	}
}
