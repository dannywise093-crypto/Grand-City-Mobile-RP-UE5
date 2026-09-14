using UnrealBuildTool;

public class GrandCityMobileEditorTarget : TargetRules
{
    public GrandCityMobileEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
        ExtraModuleNames.Add("GrandCityMobile");
    }
}
