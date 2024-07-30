// Copyright GeoTech BV

using UnrealBuildTool;

public class GFPakExporterCommandlet : ModuleRules
{
    public GFPakExporterCommandlet(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "UnrealEd",
                "GFPakExporter",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "Projects",
                "AssetRegistry",
                "AssetTools",
                "UnrealEd",
                "DeveloperToolSettings",
                "PluginUtils",
            }
        );
    }
}