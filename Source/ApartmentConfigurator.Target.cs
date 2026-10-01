using UnrealBuildTool;

public class ApartmentConfiguratorTarget : TargetRules
{
	public ApartmentConfiguratorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "ApartmentConfigurator", "ConfiguratorData" });
	}
}
