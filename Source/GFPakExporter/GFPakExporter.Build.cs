// Copyright GeoTech BV

using UnrealBuildTool;

public class GFPakExporter : ModuleRules
{
    public GFPakExporter(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "LauncherServices",
                "Json",
                "EditorSubsystem",
                "UnrealEd",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "ContentBrowserData",
                "ContentBrowserAssetDataSource",
                "ToolMenus",
                "Projects",
                "GFPakLoader",
                "LauncherServices",
                "JsonUtilities",
                "PluginUtils",
                "AssetRegistry",
                "ContentBrowser",
                "ToolWidgets",
            }
        );
    }
}