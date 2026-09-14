using UnrealBuildTool;

public class OpenAI : ModuleRules
{
    public OpenAI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "HTTP",
            "Json",
            "JsonUtilities",
            "ImageWrapper"
        });

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            // Runtime source must remain free of Win32/Win64-only dependencies.
            PublicDefinitions.Add("GRANDCITY_OPENAI_ANDROID=1");
        }
        else
        {
            PublicDefinitions.Add("GRANDCITY_OPENAI_ANDROID=0");
        }
    }
}
