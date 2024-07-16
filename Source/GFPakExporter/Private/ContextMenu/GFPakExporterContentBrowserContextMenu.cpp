// Copyright GeoTech BV

#include "GFPakExporterContentBrowserContextMenu.h"

#include "AuroraExporterConfig.h"
#include "ContentBrowserAssetDataCore.h"
#include "ContentBrowserAssetDataPayload.h"
#include "ContentBrowserDataMenuContexts.h"
#include "GFPakExporter.h"
#include "GFPakExporterLog.h"
#include "GFPakLoaderSubsystem.h"
#include "IContentBrowserSingleton.h"
#include "ILauncherServicesModule.h"
#include "ITargetDeviceServicesModule.h"
#include "Interfaces/IPluginManager.h"


#define LOCTEXT_NAMESPACE "FGFPakExporterModule"

ILauncherPtr FGFPakExporterContentBrowserContextMenu::Launcher {};
ILauncherWorkerPtr FGFPakExporterContentBrowserContextMenu::LauncherWorker {};


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
	

	// Remove Pak Plugins from the selected Plugins
	TArray<TSharedRef<IPlugin>> SelectedPlugins;
	TArray<TSharedRef<IPlugin>> AllPlugins;
	GetPluginsFromSelectedFilesAndFolders(SelectedPackagePaths, SelectedAssets, SelectedPlugins, AllPlugins);
	
	FText ErrorMessage;
	UGFPakLoaderSubsystem::Get()->EnumeratePakPluginsWithStatus<UGFPakLoaderSubsystem::EComparison::GreaterOrEqual>(EGFPakLoaderStatus::Mounted,
	[&AllPlugins, &ErrorMessage](UGFPakPlugin* PakPlugin)
	{
		if (PakPlugin->GetPluginInterface())
		{
			if (AllPlugins.Contains(PakPlugin->GetPluginInterface().ToSharedRef()))
			{
				ErrorMessage = LOCTEXT("GFPakExporter_CreateDLC_Warning_SelectedPakPlugin", "\nUnable to create a DLC with content from existing of Pak Plugins.");
				return UGFPakLoaderSubsystem::EForEachResult::Break;
			}
		}
		return AllPlugins.IsEmpty() ? UGFPakLoaderSubsystem::EForEachResult::Break : UGFPakLoaderSubsystem::EForEachResult::Continue;
	});

	// Create the Config
	bool bIsPluginDLC = SelectedAssets.IsEmpty() && SelectedPackagePaths.Num() == 1 && SelectedPlugins.Num() == 1;
	bool bCanCreateDLC = ErrorMessage.IsEmpty();
	
	FAuroraExporterConfig Config; //todo: a Config should be able to self assess if they contain DLC packs and if they are Plugin DLCs 
	Config.DLCName = bIsPluginDLC ? SelectedPlugins[0]->GetName() : TEXT("TestContentDLC"); //todo: make this an UI option
	Algo::Transform(SelectedPlugins, Config.Plugins, [](const TSharedRef<IPlugin>& Plugin){ return FName{Plugin->GetName()}; });
	Algo::Transform(SelectedPackagePaths, Config.PackagePaths, [](const FString& Path){ return FName{Path}; });
	Config.Assets = SelectedAssets;
	
	if (Config.IsEmpty())
	{
		return;
	}

	// Create the menu
	FToolMenuSection& Section = InMenu->AddSection(TEXT("GFPakExporterActions"), LOCTEXT("GFPakExporterActionsMenuHeading", "Aurora"));
	Section.InsertPosition = FToolMenuInsert(TEXT("PathViewFolderOptions"), EToolMenuInsertType::Before);
	
	{
		const FText PluginOrContent = FText::FromString(bIsPluginDLC ? TEXT("Plugin") : TEXT("Content"));
		
		FToolMenuEntry& Menu = Section.AddMenuEntry(
			TEXT("GFPakExporter_CreateDLC_MenuName"),
			FText::Format(LOCTEXT("GFPakExporter_CreateDLC_MenuEntry", "Create a {0} DLC Pak"), PluginOrContent),
			FText::Format(LOCTEXT("GFPakExporter_CreateContentDLC_MenuEntryTooltip", "Create a cooked DLC Pak of the selected {0}. {1}"),PluginOrContent, ErrorMessage),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"),
			FUIAction(
				bIsPluginDLC ?
					FExecuteAction::CreateStatic(&FGFPakExporterContentBrowserContextMenu::ExecuteCreateAuroraPluginDLCAction, Config.DLCName) :
					FExecuteAction::CreateStatic(&FGFPakExporterContentBrowserContextMenu::ExecuteCreateAuroraContentDLCAction, Config),
				FCanExecuteAction::CreateLambda([bCanCreateDLC]() { return bCanCreateDLC; })
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

void FGFPakExporterContentBrowserContextMenu::ExecuteCreateAuroraPluginDLCAction(FString InPluginName)
{
	UE_LOG(LogGFPakExporter, Display, TEXT("FGFPakExporterContentBrowserContextMenu::ExecuteExportAction - Plugin name %s"), *InPluginName);
	
	ILauncherServicesModule* LauncherServicesModule = FModuleManager::GetModulePtr<ILauncherServicesModule>(TEXT("LauncherServices"));
	if (!LauncherServicesModule)
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("Unable to get the 'LauncherServices' module"));
		return;
	}
	
	TSharedRef<ILauncherProfileManager> LauncherProfileManager = LauncherServicesModule->GetProfileManager();
	TArray<ITargetPlatform*> Platforms = GetTargetPlatformManager()->GetTargetPlatforms();
	ITargetDeviceServicesModule& DeviceServiceModule = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>(TEXT("TargetDeviceServices"));
	TSharedRef<ITargetDeviceProxyManager> DeviceProxyManager = DeviceServiceModule.GetDeviceProxyManager();
	
	// Platforms[0].
	ILauncherProfileRef Profile = LauncherProfileManager->CreateUnsavedProfile(FString::Printf(TEXT("AuroraDLC-%s"), *InPluginName));
	Profile->SetDefaults();

	Profile->SetProjectSpecified(true);
	// Build
	Profile->SetBuildConfiguration(EBuildConfiguration::Development); // todo: setting
	// Cook
	Profile->SetCookConfiguration(EBuildConfiguration::Development); // todo: setting
	Profile->SetCookMode(ELauncherProfileCookModes::ByTheBook);
	Profile->AddCookedPlatform(TEXT("Windows")); // todo: setting
	// Cook - Release DLC
	Profile->SetBasedOnReleaseVersionName(TEXT("AuroraDummyReleaseVersion")); // todo: setting
	Profile->SetCreateDLC(true);
	Profile->SetDLCName(InPluginName);
	Profile->SetDLCIncludeEngineContent(true); //todo: setting
	// Cook - Advanced
	Profile->SetUnversionedCooking(false); // todo: setting
	Profile->SetDeployWithUnrealPak(true);

	Profile->SetUseZenStore(false);

	FString AssetRegistryFolder = FGFPakExporterModule::GetTempAssetRegistryDir();
	FString CookOptions = FString::Printf(TEXT("-%s=\"%s\""), *FGFPakExporterModule::AuroraCommandLineParameter, *InPluginName);
	CookOptions += TEXT(" -BasedOnReleaseVersion=\"AuroraDummyReleaseVersion\""); // Not needed, will be ignored
	CookOptions += FString::Printf(TEXT(" -BasedOnReleaseVersionRoot=\"%s\""), *AssetRegistryFolder); // Folder containing our custom Asset Registry
	CookOptions += TEXT(" -DevelopmentAssetRegistryPlatformOverride=..\\"); // Need to be the parent folder to disregard the BasedOnReleaseVersion
	CookOptions += TEXT(" -CookSkipRequests -CookSkipSoftRefs -CookSkipHardRefs -DisableUnsolicitedPackages -NoGameAlwaysCookPackages -SkipZenStore");
	Profile->SetCookOptions(CookOptions);
	
	// Package
	Profile->SetPackagingMode(ELauncherProfilePackagingModes::Locally);
	// Deploy
	Profile->SetDeploymentMode(ELauncherProfileDeploymentModes::DoNotDeploy);
	
	if (Profile->IsValidForLaunch())
	{
		Launcher = LauncherServicesModule->CreateLauncher();
		LauncherWorker = Launcher->Launch(DeviceProxyManager, Profile);

		// This will allow us to pipe the launcher messages into the command window.
		LauncherWorker->OnOutputReceived().AddStatic(&FGFPakExporterContentBrowserContextMenu::MessageReceived);
		// Allows us to exit this command once the launcher worker has completed or is canceled
		LauncherWorker->OnStageStarted().AddStatic(&FGFPakExporterContentBrowserContextMenu::HandleStageStarted);
		LauncherWorker->OnStageCompleted().AddStatic(&FGFPakExporterContentBrowserContextMenu::HandleStageCompleted);
		LauncherWorker->OnCompleted().AddStatic(&FGFPakExporterContentBrowserContextMenu::LaunchCompleted);
		LauncherWorker->OnCanceled().AddStatic(&FGFPakExporterContentBrowserContextMenu::LaunchCanceled);

		TArray<ILauncherTaskPtr> TaskList;
		int32 NumOfTasks = LauncherWorker->GetTasks(TaskList);	
		UE_LOG(LogGFPakExporter, Display, TEXT("There are '%i' tasks to be completed."), NumOfTasks);
	}
	else
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("Launcher profile '%s' for is not valid for launch."),
			*Profile->GetName());
		for (int32 i = 0; i < (int32)ELauncherProfileValidationErrors::Count; ++i)
		{
			ELauncherProfileValidationErrors::Type Error = (ELauncherProfileValidationErrors::Type)i;
			if (Profile->HasValidationError(Error))
			{
				UE_LOG(LogGFPakExporter, Error, TEXT("ValidationError: %s"), *LexToStringLocalized(Error));
			}
		}
	}
}

void FGFPakExporterContentBrowserContextMenu::ExecuteCreateAuroraContentDLCAction(FAuroraExporterConfig InConfig)
{
	UE_LOG(LogGFPakExporter, Display, TEXT("FGFPakExporterContentBrowserContextMenu::ExecuteCreateAuroraContentDLCAction"));
	
	if (!InConfig.IsValid())
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("The given Config is invalid (either empty or no DLCName given)"));
		return;
	}

	FString TempDir = FGFPakExporterModule::GetPluginTempDir();
	FString ConfigFilename = FPaths::Combine(TempDir, TEXT("AuroraExporterConfig.json"));
	if (!InConfig.SaveJsonConfig(ConfigFilename))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("Unable save the 'AuroraExporterConfig' to '%s'"), *ConfigFilename);
		return;
	}
	
	ILauncherServicesModule* LauncherServicesModule = FModuleManager::GetModulePtr<ILauncherServicesModule>(TEXT("LauncherServices"));
	if (!LauncherServicesModule)
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("Unable to get the 'LauncherServices' module"));
		return;
	}
	
	TSharedRef<ILauncherProfileManager> LauncherProfileManager = LauncherServicesModule->GetProfileManager();
	TArray<ITargetPlatform*> Platforms = GetTargetPlatformManager()->GetTargetPlatforms();
	ITargetDeviceServicesModule& DeviceServiceModule = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>(TEXT("TargetDeviceServices"));
	TSharedRef<ITargetDeviceProxyManager> DeviceProxyManager = DeviceServiceModule.GetDeviceProxyManager();
	
	ILauncherProfileRef Profile = LauncherProfileManager->CreateUnsavedProfile(FString::Printf(TEXT("AuroraDLC-%s"), *InConfig.DLCName));
	Profile->SetDefaults();

	Profile->SetProjectSpecified(true);
	// Build
	Profile->SetBuildConfiguration(EBuildConfiguration::Development); // todo: setting
	// Cook
	Profile->SetCookConfiguration(EBuildConfiguration::Development); // todo: setting
	Profile->SetCookMode(ELauncherProfileCookModes::ByTheBook);
	Profile->AddCookedPlatform(TEXT("Windows")); // todo: setting
	// Cook - Release DLC
	Profile->SetBasedOnReleaseVersionName(TEXT("AuroraDummyReleaseVersion")); // todo: setting
	Profile->SetCreateDLC(true);
	Profile->SetDLCName(InConfig.DLCName);
	Profile->SetDLCIncludeEngineContent(true); //todo: setting
	// Cook - Advanced
	Profile->SetUnversionedCooking(false); // todo: setting
	Profile->SetDeployWithUnrealPak(true);
	
	Profile->SetUseZenStore(false);

	FString AssetRegistryFolder = FGFPakExporterModule::GetTempAssetRegistryDir();
	FString CookOptions = FString::Printf(TEXT("-%s=\"%s\""), *FGFPakExporterModule::AuroraCommandLineParameter, *ConfigFilename);
	CookOptions += TEXT(" -BasedOnReleaseVersion=\"AuroraDummyReleaseVersion\""); // Not needed, will be ignored
	CookOptions += FString::Printf(TEXT(" -BasedOnReleaseVersionRoot=\"%s\""), *AssetRegistryFolder); // Folder containing our custom Asset Registry
	CookOptions += TEXT(" -DevelopmentAssetRegistryPlatformOverride=..\\"); // Need to be the parent folder to disregard the BasedOnReleaseVersion
	CookOptions += TEXT(" -CookSkipRequests -CookSkipSoftRefs -CookSkipHardRefs -DisableUnsolicitedPackages -NoGameAlwaysCookPackages -SkipZenStore");
	Profile->SetCookOptions(CookOptions);
	
	// Package
	Profile->SetPackagingMode(ELauncherProfilePackagingModes::Locally);
	// Deploy
	Profile->SetDeploymentMode(ELauncherProfileDeploymentModes::DoNotDeploy);
	
	if (Profile->IsValidForLaunch())
	{
		Launcher = LauncherServicesModule->CreateLauncher();
		LauncherWorker = Launcher->Launch(DeviceProxyManager, Profile);

		// This will allow us to pipe the launcher messages into the command window.
		LauncherWorker->OnOutputReceived().AddStatic(&FGFPakExporterContentBrowserContextMenu::MessageReceived);
		// Allows us to exit this command once the launcher worker has completed or is canceled
		LauncherWorker->OnStageStarted().AddStatic(&FGFPakExporterContentBrowserContextMenu::HandleStageStarted);
		LauncherWorker->OnStageCompleted().AddStatic(&FGFPakExporterContentBrowserContextMenu::HandleStageCompleted);
		LauncherWorker->OnCompleted().AddStatic(&FGFPakExporterContentBrowserContextMenu::LaunchCompleted);
		LauncherWorker->OnCanceled().AddStatic(&FGFPakExporterContentBrowserContextMenu::LaunchCanceled);

		TArray<ILauncherTaskPtr> TaskList;
		int32 NumOfTasks = LauncherWorker->GetTasks(TaskList);	
		UE_LOG(LogGFPakExporter, Display, TEXT("There are '%i' tasks to be completed."), NumOfTasks);
	}
	else
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("Launcher profile '%s' for is not valid for launch."),
			*Profile->GetName());
		for (int32 i = 0; i < (int32)ELauncherProfileValidationErrors::Count; ++i)
		{
			ELauncherProfileValidationErrors::Type Error = (ELauncherProfileValidationErrors::Type)i;
			if (Profile->HasValidationError(Error))
			{
				UE_LOG(LogGFPakExporter, Error, TEXT("ValidationError: %s"), *LexToStringLocalized(Error));
			}
		}
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


void FGFPakExporterContentBrowserContextMenu::MessageReceived(const FString& InMessage)
{
	GLog->Logf(ELogVerbosity::Log, TEXT("%s"), *InMessage);
}

void FGFPakExporterContentBrowserContextMenu::HandleStageStarted(const FString& InStage)
{
	UE_LOG(LogGFPakExporter, Warning, TEXT("Starting stage %s."), *InStage);
}

void FGFPakExporterContentBrowserContextMenu::HandleStageCompleted(const FString& InStage, double StageTime)
{
	UE_LOG(LogGFPakExporter, Warning, TEXT("Completed Stage %s."), *InStage);
}

void FGFPakExporterContentBrowserContextMenu::LaunchCompleted(bool Outcome, double ExecutionTime, int32 ReturnCode)
{
	UE_LOG(LogGFPakExporter, Log, TEXT("Profile launch command %s."), Outcome ? TEXT("is SUCCESSFUL") : TEXT("has FAILED"));
}

void FGFPakExporterContentBrowserContextMenu::LaunchCanceled(double ExecutionTime)
{
	UE_LOG(LogGFPakExporter, Log, TEXT("Profile launch command was canceled."));
}

#undef LOCTEXT_NAMESPACE
