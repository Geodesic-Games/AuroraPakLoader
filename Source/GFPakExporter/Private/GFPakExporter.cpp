// Copyright GeoTech BV

#include "GFPakExporter.h"

#include "AuroraSaveFilePath.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContextMenu/GFPakExporterContentBrowserContextMenu.h"
#include "DetailsViewCustomization/AuroraDirectoryPathStructCustomization.h"
#include "DetailsViewCustomization/AuroraSaveFilePathStructCustomization.h"


#define LOCTEXT_NAMESPACE "FGFPakExporterModule"


const FName FGFPakExporterModule::ModuleName{TEXT("GFPakExporter")};
const FString FGFPakExporterModule::AuroraContentDLCCommandLineParameter{TEXT("AuroraDLCConfig")};
const FString FGFPakExporterModule::AuroraBaseGameCommandLineParameter{TEXT("AuroraBaseGameConfig")};


void FGFPakExporterModule::StartupModule()
{
	FDelayedAutoRegisterHelper(EDelayedRegisterRunPhase::EndOfEngineInit, [this]()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		RegisterPropertyTypeCustomizations();
		
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
	UnregisterPropertyTypeCustomizations();
}



TArray<FName> FGFPakExporterModule::GetAssetDependencies(const TArray<FSoftObjectPath>& Assets)
{
	TArray<FName> PackageNames;
	Algo::Transform(Assets, PackageNames, [](const FSoftObjectPath& Path) { return Path.GetLongPackageFName();});
	return GetAssetDependencies(PackageNames);
}
TArray<FName> FGFPakExporterModule::GetAssetDependencies(const TArray<FName>& PackageNames)
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
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
					PackagesToBeProcessed.Add(Dependency.AssetId.PackageName);
				}
			}
		}
	}
	
	return ProcessedPackages.Array();
}

void FGFPakExporterModule::RegisterPropertyTypeCustomizations()
{
	static FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);
	PropertyModule.RegisterCustomPropertyTypeLayout(FAuroraSaveFilePath::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAuroraSaveFilePathStructCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(FAuroraDirectoryPath::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAuroraDirectoryPathStructCustomization::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();
}

void FGFPakExporterModule::UnregisterPropertyTypeCustomizations()
{
	static FName PropertyEditor("PropertyEditor");
	if (UObjectInitialized() && FModuleManager::Get().IsModuleLoaded(PropertyEditor))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout( FAuroraSaveFilePath::StaticStruct()->GetFName() );
		PropertyModule.UnregisterCustomPropertyTypeLayout( FAuroraDirectoryPath::StaticStruct()->GetFName() );
		PropertyModule.NotifyCustomizationModuleChanged();
	}
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FGFPakExporterModule, GFPakExporter)