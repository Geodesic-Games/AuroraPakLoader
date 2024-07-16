// Copyright GeoTech BV

#include "GFPakExporter.h"

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

FString FGFPakExporterModule::GetPluginTempDir()
{
	return FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("AuroraExporter"));
}

FString FGFPakExporterModule::GetTempAssetRegistryDir()
{
	return FPaths::Combine(GetPluginTempDir(), TEXT("AssetRegistry"));
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FGFPakExporterModule, GFPakExporter)