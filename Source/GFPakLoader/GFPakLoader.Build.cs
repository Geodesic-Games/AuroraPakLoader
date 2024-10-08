// Copyright GeoTech BV

using UnrealBuildTool;

public class GFPakLoader : ModuleRules
{
	public GFPakLoader(ReadOnlyTargetRules Target) : base(Target)
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
				"Core",
				"AssetRegistry",
				"Projects",
				"GameFeatures",
			}
		);
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
                "DeveloperSettings",
                "Engine",
                "PakFile",
                "Projects", 
                "EngineSettings",
                "RenderCore",
                "RHI",
            }
		);
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
			PrivateDependencyModuleNames.Add( "TypedElementRuntime");
			PrivateDependencyModuleNames.Add( "ContentBrowserData");
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
}
