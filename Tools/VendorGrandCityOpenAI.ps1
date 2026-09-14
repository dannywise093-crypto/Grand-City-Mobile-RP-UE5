param(
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")),
    [string]$SourceZip = ""
)

$ErrorActionPreference = "Stop"
$pluginRoot = Join-Path $ProjectRoot "Plugins/GrandCityOpenAI"
$sourceRoot = Join-Path $pluginRoot "Source"
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) "GrandCityOpenAI"

if (Test-Path $tempRoot) { Remove-Item $tempRoot -Recurse -Force }
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

if ([string]::IsNullOrWhiteSpace($SourceZip)) {
    throw "Pass -SourceZip pointing to the supplied UnrealOpenAIPlugin ZIP. The source is intentionally not downloaded implicitly so the vendored version is explicit and auditable."
}

if (-not (Test-Path $SourceZip)) { throw "Source ZIP not found: $SourceZip" }

Expand-Archive -LiteralPath $SourceZip -DestinationPath $tempRoot -Force
$sourcePlugin = Join-Path $tempRoot "Source/OpenAI"
if (-not (Test-Path $sourcePlugin)) { throw "Archive does not contain Source/OpenAI" }

New-Item -ItemType Directory -Force -Path $sourceRoot | Out-Null
Copy-Item (Join-Path $sourcePlugin "*") $sourceRoot -Recurse -Force

# Copy only the runtime module. Editor/test modules and prebuilt Win64 binaries are excluded.
$runtimeBuild = Join-Path $sourceRoot "OpenAI/OpenAI.Build.cs"
if (-not (Test-Path $runtimeBuild)) { throw "OpenAI.Build.cs missing after extraction" }

# Force the mobile-safe module dependencies used by the Grand City port.
@"
using UnrealBuildTool;

public class OpenAI : ModuleRules
{
    public OpenAI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "HTTP", "Json", "JsonUtilities", "ImageWrapper" });
        PublicDefinitions.Add(Target.Platform == UnrealTargetPlatform.Android ? "GRANDCITY_OPENAI_ANDROID=1" : "GRANDCITY_OPENAI_ANDROID=0");
    }
}
"@ | Set-Content -Encoding UTF8 $runtimeBuild

Write-Host "Grand City OpenAI runtime source vendored into $sourceRoot"
Write-Host "Next: regenerate UE5.6 project files and compile Win64 Editor, then Android Development/Shipping."
