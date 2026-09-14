using UnrealBuildTool;

public class GrandCityMobileTarget : TargetRules
{
    public GrandCityMobileTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
        ExtraModuleNames.Add("GrandCityMobile");
    }
}
