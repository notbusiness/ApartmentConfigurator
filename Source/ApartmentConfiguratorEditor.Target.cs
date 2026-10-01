using UnrealBuildTool;

public class ApartmentConfiguratorEditorTarget : TargetRules
{
	public ApartmentConfiguratorEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "ApartmentConfigurator", "ConfiguratorData" });
	}
}
