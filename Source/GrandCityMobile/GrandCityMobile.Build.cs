using UnrealBuildTool;

public class GrandCityMobile : ModuleRules
{
    public GrandCityMobile(ReadOnlyTargetRules Target) : base(Target)
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
            "Slate",
            "AIModule",
            "StateTreeModule",
            "GameplayStateTreeModule",
            "NavigationSystem",
            "NetCore",
            "HTTP",
            "Json",
            "JsonUtilities"
        });
        PrivateDependencyModuleNames.Add("CoreOnline");

        PublicIncludePaths.AddRange(new string[]
        {
            "GrandCityMobile",
            "GrandCityMobile/Variant_Platforming",
            "GrandCityMobile/Variant_Platforming/Animation",
            "GrandCityMobile/Variant_Combat",
            "GrandCityMobile/Variant_Combat/AI",
            "GrandCityMobile/Variant_Combat/Animation",
            "GrandCityMobile/Variant_Combat/Gameplay",
            "GrandCityMobile/Variant_Combat/Interfaces",
            "GrandCityMobile/Variant_Combat/UI",
            "GrandCityMobile/Variant_SideScrolling",
            "GrandCityMobile/Variant_SideScrolling/AI",
            "GrandCityMobile/Variant_SideScrolling/Gameplay",
            "GrandCityMobile/Variant_SideScrolling/Interfaces",
            "GrandCityMobile/Variant_SideScrolling/UI"
        });
    }
}
