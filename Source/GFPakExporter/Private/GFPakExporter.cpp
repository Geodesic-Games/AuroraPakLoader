#include "GFPakExporter.h"
#include "ContextMenu/GFPakExporterAssetFolderContextMenu.h"

#define LOCTEXT_NAMESPACE "FGFPakExporterModule"

void FGFPakExporterModule::StartupModule()
{
	// Create the asset menu instances
	AssetFolderContextMenu = MakeShared<FGFPakExporterAssetFolderContextMenu>();
	AssetFolderContextMenu->Initialize();

	//todo: check FPluginBrowserModule::Get().GetCustomizePluginEditingDelegates() to add to the plugin's edit menu
}

void FGFPakExporterModule::ShutdownModule()
{
	if (AssetFolderContextMenu.IsValid())
	{
		AssetFolderContextMenu->Shutdown();
		AssetFolderContextMenu.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FGFPakExporterModule, GFPakExporter)