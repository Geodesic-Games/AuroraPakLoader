// Copyright GeoTech BV

using UnrealBuildTool;

public class GFPakLoaderEditor : ModuleRules
{
	public GFPakLoaderEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[]
			{
				// ... add public include paths required here ...
			}
		);
		PrivateIncludePaths.AddRange(
			new string[]
			{
				// ... add other private include paths required here ...
			}
		);
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
		);
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
                "DeveloperSettings",
                "Engine",
				"Slate",
				"SlateCore",

				"GFPakLoader"
			}
		);
    }
}
