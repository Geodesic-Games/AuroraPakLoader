// Copyright GeoTech BV

#include "GFPakExporter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ContextMenu/GFPakExporterContentBrowserContextMenu.h"


#define LOCTEXT_NAMESPACE "FGFPakExporterModule"


const FName FGFPakExporterModule::ModuleName{TEXT("GFPakExporter")};
const FString FGFPakExporterModule::AuroraCommandLineParameter{TEXT("AuroraDLCConfig")};


void FGFPakExporterModule::StartupModule()
{
	FDelayedAutoRegisterHelper(EDelayedRegisterRunPhase::EndOfEngineInit, [this]()
	{
		// Create the asset menu instances
		ContentBrowserContextMenu = MakeShared<FGFPakExporterContentBrowserContextMenu>();
		ContentBrowserContextMenu->Initialize();

		//todo: check FPluginBrowserModule::Get().GetCustomizePluginEditingDelegates() to add to the plugin's edit menu
	});
}

void FGFPakExporterModule::ShutdownModule()
{
	if (ContentBrowserContextMenu.IsValid())
	{
		ContentBrowserContextMenu->Shutdown();
		ContentBrowserContextMenu.Reset();
	}
}



TArray<FName> FGFPakExporterModule::GetAssetDependencies(const TArray<FSoftObjectPath>& Assets)
{
	TArray<FName> PackageNames;
	Algo::Transform(Assets, PackageNames, [](const FSoftObjectPath& Path) { return Path.GetLongPackageFName();});
	return GetAssetDependencies(PackageNames);
}
TArray<FName> FGFPakExporterModule::GetAssetDependencies(const TArray<FName>& PackageNames)
{
	// TArray<FName> OutDependencies;
	
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	// TSet<FName> AllDependencyNamesSet;
	// TMap<UE::AssetRegistry::EDependencyProperty, TArray<FAssetDependency>> PropDependency;
	// TMap<UE::AssetRegistry::EDependencyCategory, TArray<FAssetDependency>> CatDependency;
	// TMap<FName, FAssetDependency> DependencyMap;
	
	TSet<FName> ProcessedPackages;
	TArray<FName> PackagesToBeProcessed{PackageNames};
	while(!PackagesToBeProcessed.IsEmpty())
	{
		FName PackageName = PackagesToBeProcessed.Pop();
		
		bool bIsAlreadyInSet;
		ProcessedPackages.Add(PackageName, &bIsAlreadyInSet);
		if (bIsAlreadyInSet)
		{
			continue;
		}
		
		const FAssetIdentifier AssetIdentifier(PackageName);
		TArray<FAssetDependency> AssetDependencies;
		// todo: There are a few Dependency Categories/Properties, not fully sure if any should be discarded
		if (AssetRegistry.GetDependencies(AssetIdentifier, AssetDependencies, UE::AssetRegistry::EDependencyCategory::All))
		{
			for (const FAssetDependency& Dependency : AssetDependencies)
			{
				// Exclude script/memory packages
				if (FPackageName::IsValidLongPackageName(Dependency.AssetId.PackageName.ToString()))
				{
					// PropDependency.FindOrAdd(Dependency.Properties).Add(Dependency);
					// CatDependency.FindOrAdd(Dependency.Category).Add(Dependency);
					//
					// FAssetDependency& Dep = DependencyMap.FindOrAdd(Dependency.AssetId.PackageName);
					// Dep.AssetId = Dependency.AssetId;
					// Dep.Properties |= Dependency.Properties;
					// Dep.Category |= Dependency.Category;
					
					// AllDependencyNamesSet.Add(Dependency.AssetId.PackageName);
					PackagesToBeProcessed.Add(Dependency.AssetId.PackageName);
				}
			}
		}
	}

	// TArray<FName> AllDependencyNames = AllDependencyNamesSet.Array();
	// OutDependencies.Sort(FNameLexicalLess());
	// AllDependencyNames.Sort(FNameLexicalLess());
	
	// OutDependencies = ProcessedPackages.Array();
	return ProcessedPackages.Array();
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FGFPakExporterModule, GFPakExporter)