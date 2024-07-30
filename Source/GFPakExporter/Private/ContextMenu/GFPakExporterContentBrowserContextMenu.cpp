// Copyright GeoTech BV

#include "GFPakExporterContentBrowserContextMenu.h"

#include "AuroraExporterSettings.h"
#include "ContentBrowserAssetDataCore.h"
#include "ContentBrowserAssetDataPayload.h"
#include "ContentBrowserDataMenuContexts.h"
#include "GFPakExporter.h"
#include "GFPakExporterLog.h"
#include "GFPakExporterSubsystem.h"
#include "IContentBrowserSingleton.h"
#include "ILauncherServicesModule.h"
#include "ITargetDeviceServicesModule.h"
#include "Interfaces/IPluginManager.h"


#define LOCTEXT_NAMESPACE "FGFPakExporterModule"


void FGFPakExporterContentBrowserContextMenu::Initialize()
{
	UE_LOG(LogGFPakExporter, Verbose, TEXT("FGFPakExporterContentBrowserContextMenu::Initialize - ..."));
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateSP(this, &FGFPakExporterContentBrowserContextMenu::RegisterMenus));
}

void FGFPakExporterContentBrowserContextMenu::Shutdown()
{
}

void FGFPakExporterContentBrowserContextMenu::RegisterMenus()
{
	UE_LOG(LogGFPakExporter, Verbose, TEXT("FGFPakExporterContentBrowserContextMenu::RegisterMenus"));

	if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.FolderContextMenu"))
	{
		Menu->AddDynamicSection(TEXT("DynamicSection_GFPakExporter_ContextMenu_Folder"),
			FNewToolMenuDelegate::CreateLambda([WeakThis = AsWeak()](UToolMenu* InMenu)
		{
			if (const TSharedPtr<FGFPakExporterContentBrowserContextMenu> This = WeakThis.Pin())
			{
				constexpr bool bIsAssetMenu = false;
				This->PopulateContextMenu(InMenu, bIsAssetMenu);
			}
		}));
	}

	if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu"))
	{
		Menu->AddDynamicSection(TEXT("DynamicSection_GFPakExporter_ContextMenu_Asset"),
			FNewToolMenuDelegate::CreateLambda([WeakThis = AsWeak()](UToolMenu* InMenu)
		{
			if (const TSharedPtr<FGFPakExporterContentBrowserContextMenu> This = WeakThis.Pin())
			{
				constexpr bool bIsAssetMenu = true;
				This->PopulateContextMenu(InMenu, bIsAssetMenu);
			}
		}));
	}
}

void FGFPakExporterContentBrowserContextMenu::PopulateContextMenu(UToolMenu* InMenu, bool bIsAssetMenu) const
{
	UE_LOG(LogGFPakExporter, Verbose, TEXT("FGFPakExporterContentBrowserContextMenu::PopulateAssetFolderContextMenu - InMenu: %s  IsAssetMenu? %s"),
		*GetNameSafe(InMenu), bIsAssetMenu ? TEXT("True") : TEXT("False"));
	check(InMenu);
	
	// Extract the internal package paths that belong to this data source from the full list of selected items given in the context
	TArray<FString> SelectedPackagePaths;
	TArray<FSoftObjectPath> SelectedAssets;
	GetSelectedFilesAndFolders(InMenu, SelectedPackagePaths, SelectedAssets);

	UE_LOG(LogGFPakExporter, Verbose, TEXT("FGFPakExporterContentBrowserContextMenu::PopulateAssetFolderContextMenu - Result ..."));
	UE_LOG(LogGFPakExporter, Verbose, TEXT("\tSelectedPackagePaths: %d"), SelectedPackagePaths.Num());
	for (const FString& SelectedPackagePath : SelectedPackagePaths)
	{
		UE_LOG(LogGFPakExporter, Verbose, TEXT("\t- %s"), *SelectedPackagePath);
	}

	UE_LOG(LogGFPakExporter, Verbose, TEXT("\tSelectedAssets: %d"), SelectedAssets.Num());
	for (const FSoftObjectPath& SelectedAsset : SelectedAssets)
	{
		UE_LOG(LogGFPakExporter, Verbose, TEXT("\t- %s"), *SelectedAsset.ToString());
	}
	
	FAuroraExporterConfig Config;
	Algo::Transform(SelectedPackagePaths, Config.PackagePaths, [](const FString& Path){ return FAuroraDirectoryPath{Path}; });
	Config.Assets = SelectedAssets;
	
	Config.DLCName = Config.GetDefaultDLCNameBasedOnContent(TEXT("DLCPak"));
	
	if (Config.IsEmpty())
	{
		return;
	}

	// Create the menu
	FToolMenuSection& Section = InMenu->AddSection(TEXT("GFPakExporterActions"), LOCTEXT("GFPakExporterActionsMenuHeading", "Aurora"));
	if (bIsAssetMenu)
	{
		Section.InsertPosition = FToolMenuInsert(TEXT("AssetContextExploreMenuOptions"), EToolMenuInsertType::After);
	}
	else
	{
		Section.InsertPosition = FToolMenuInsert(TEXT("PathViewFolderOptions"), EToolMenuInsertType::Before);
	}
	
	{
		FToolMenuEntry& Menu = Section.AddMenuEntry(
			TEXT("GFPakExporter_CreateDLC_MenuName"),
			LOCTEXT("GFPakExporter_CreateDLC_MenuEntry", "Create a Content DLC Pak"),
			LOCTEXT("GFPakExporter_CreateContentDLC_MenuEntryTooltip", "Create a cooked DLC Pak of the selected Content."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"),
			FUIAction(FExecuteAction::CreateStatic(&FGFPakExporterContentBrowserContextMenu::ExecuteCreateAuroraContentDLCAction, Config),
				FCanExecuteAction::CreateLambda([]() { return (bool)UGFPakExporterSubsystem::Get(); })
			)
		);
	}
}

void FGFPakExporterContentBrowserContextMenu::GetSelectedFilesAndFolders(const UToolMenu* InMenu, TArray<FString>& OutSelectedPackagePaths, TArray<FSoftObjectPath>& OutSelectedAssets)
{
	UE_LOG(LogGFPakExporter, Verbose, TEXT("FGFPakExporterContentBrowserContextMenu::GetSelectedFilesAndFolders - InMenu: %s"), *GetNameSafe(InMenu));
	check(InMenu);
	
	auto AddSelectedItem = [&OutSelectedPackagePaths, &OutSelectedAssets](const FContentBrowserItem& SelectedItem)
	{
		const FContentBrowserItemData* SelectedItemData = SelectedItem.GetPrimaryInternalItem();
		if (!SelectedItemData)
		{
			return;
		}

		const UContentBrowserDataSource* DataSource = SelectedItemData->GetOwnerDataSource();
		if (!DataSource)
		{
			return;
		}

		for (const FContentBrowserItemData& InternalItems : SelectedItem.GetInternalItems())
		{
			if (const TSharedPtr<const FContentBrowserAssetFolderItemDataPayload> FolderPayload = ContentBrowserAssetData::GetAssetFolderItemPayload(DataSource, InternalItems))
			{
				OutSelectedPackagePaths.Add(FolderPayload->GetInternalPath().ToString());
			}
			else if (const TSharedPtr<const FContentBrowserAssetFileItemDataPayload> ItemPayload = ContentBrowserAssetData::GetAssetFileItemPayload(DataSource, InternalItems))
			{
				OutSelectedAssets.Add(ItemPayload->GetAssetData().GetSoftObjectPath());
			}
		}
	};
	
	if (const UContentBrowserDataMenuContext_FolderMenu* ContextObject = InMenu->FindContext<UContentBrowserDataMenuContext_FolderMenu>())
	{
		for (const FContentBrowserItem& SelectedItem : ContextObject->SelectedItems)
		{
			AddSelectedItem(SelectedItem);
		}
	}
	if (const UContentBrowserDataMenuContext_FileMenu* ContextObject = InMenu->FindContext<UContentBrowserDataMenuContext_FileMenu>())
	{
		for (const FContentBrowserItem& SelectedItem :  ContextObject->SelectedItems)
		{
			AddSelectedItem(SelectedItem);
		}
	}
}

void FGFPakExporterContentBrowserContextMenu::ExecuteCreateAuroraContentDLCAction(FAuroraExporterConfig InConfig)
{
	if (UGFPakExporterSubsystem* Subsystem = UGFPakExporterSubsystem::Get())
	{
		Subsystem->PromptForExport(InConfig);
	}
}

void FGFPakExporterContentBrowserContextMenu::GetPluginsFromSelectedFilesAndFolders(const TArray<FString>& InSelectedPackagePaths, const TArray<FSoftObjectPath>& InSelectedAssets,
	TArray<TSharedRef<IPlugin>>& OutSelectedPlugins, TArray<TSharedRef<IPlugin>>& OutAllPlugins)
{
	TSet<FString> MountPoints;
	TSet<FString> SelectedMountPoints;
	for (const FString& PackagePath : InSelectedPackagePaths)
	{
		const FName MountPoint = FPackageName::GetPackageMountPoint(PackagePath, false);
		MountPoints.Add(MountPoint.ToString());
		if (FPaths::IsSamePath(MountPoint.ToString(), PackagePath + TEXT("/")))
		{
			SelectedMountPoints.Add(MountPoint.ToString());
		}
	}
	for (const FSoftObjectPath& ObjectPath : InSelectedAssets)
	{
		const FName MountPoint = FPackageName::GetPackageMountPoint(ObjectPath.GetAssetPathString(), false);
		MountPoints.Add(MountPoint.ToString());
	}
	
	IPluginManager& PluginManager = IPluginManager::Get();
	const TArray<TSharedRef<IPlugin>> AllPlugins = PluginManager.GetDiscoveredPlugins(); // We don't care if they are enabled or not
	for (const TSharedRef<IPlugin>& Plugin : AllPlugins)
	{
		if (Plugin->CanContainContent())
		{
			const FString MountedAssetPath = Plugin->GetMountedAssetPath();
			if (MountPoints.Remove(MountedAssetPath) > 0)
			{
				OutAllPlugins.Add(Plugin);
				if (SelectedMountPoints.Remove(MountedAssetPath) > 0)
				{
					OutSelectedPlugins.Add(Plugin);
				}
				if (MountPoints.IsEmpty() && SelectedMountPoints.IsEmpty())
				{
					break;
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
