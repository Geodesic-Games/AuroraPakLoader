// Fill out your copyright notice in the Description page of Project Settings.


#include "GFPakExporterAssetFolderContextMenu.h"

#include "ContentBrowserAssetDataCore.h"
#include "ContentBrowserAssetDataPayload.h"
#include "ContentBrowserDataMenuContexts.h"
#include "GFPakExporterCommandlet.h"
#include "GFPakExporterLog.h"
#include "GFPakLoaderSubsystem.h"
#include "ILauncherServicesModule.h"
#include "ITargetDeviceServicesModule.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FGFPakExporterModule"

ILauncherPtr FGFPakExporterAssetFolderContextMenu::Launcher {};
ILauncherWorkerPtr FGFPakExporterAssetFolderContextMenu::LauncherWorker {};

void FGFPakExporterAssetFolderContextMenu::Initialize()
{
	UE_LOG(LogGFPakExporter, Verbose, TEXT("FGFPakExporterAssetFolderContextMenu::Initialize - ..."));
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateSP(this, &FGFPakExporterAssetFolderContextMenu::RegisterMenus));
}

void FGFPakExporterAssetFolderContextMenu::Shutdown()
{
}

void FGFPakExporterAssetFolderContextMenu::RegisterMenus()
{
	UE_LOG(LogGFPakExporter, Verbose, TEXT("FGFPakExporterAssetFolderContextMenu::RegisterMenus"));

	if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.FolderContextMenu"))
	{
		Menu->AddDynamicSection(TEXT("DynamicSection_GFPakExporter_ContextMenu_Folder"), FNewToolMenuDelegate::CreateLambda([WeakThis = AsWeak()](UToolMenu* InMenu)
		{
			if (const TSharedPtr<FGFPakExporterAssetFolderContextMenu> This = WeakThis.Pin())
			{
				This->PopulateAssetFolderContextMenu(InMenu);
			}
		}));
	}

	if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu"))
	{
		Menu->AddDynamicSection(TEXT("DynamicSection_GFPakExporter_ContextMenu_Asset"), FNewToolMenuDelegate::CreateLambda([WeakThis = AsWeak()](UToolMenu* InMenu)
		{
			if (const TSharedPtr<FGFPakExporterAssetFolderContextMenu> This = WeakThis.Pin())
			{
				This->PopulateAssetFileContextMenu(InMenu);
			}
		}));
	}
}

void FGFPakExporterAssetFolderContextMenu::PopulateAssetFolderContextMenu(UToolMenu* InMenu) const
{
	UE_LOG(LogGFPakExporter, Verbose, TEXT("FGFPakExporterAssetFolderContextMenu::PopulateAssetFolderContextMenu - InMenu: %s"), *GetNameSafe(InMenu));
	check(InMenu);

	const UContentBrowserDataMenuContext_FolderMenu* ContextObject = InMenu->FindContext<UContentBrowserDataMenuContext_FolderMenu>();
	checkf(ContextObject, TEXT("Required context UContentBrowserDataMenuContext_FolderMenu was missing!"));

	// Extract the internal package paths that belong to this data source from the full list of selected items given in the context
	TArray<FString> SelectedPackagePaths;
	TArray<FString> SelectedAssetPackages;
	GetSelectedFilesAndFolders(InMenu, SelectedPackagePaths, SelectedAssetPackages);

	UE_LOG(LogGFPakExporter, Verbose, TEXT("FGFPakExporterAssetFolderContextMenu::PopulateAssetFolderContextMenu - Result ..."));
	UE_LOG(LogGFPakExporter, Verbose, TEXT("\tSelectedPackagePaths: %d"), SelectedPackagePaths.Num());
	for (const FString& SelectedPackagePath : SelectedPackagePaths)
	{
		UE_LOG(LogGFPakExporter, Verbose, TEXT("\t- %s"), *SelectedPackagePath);
	}

	UE_LOG(LogGFPakExporter, Verbose, TEXT("\tSelectedAssetPackages: %d"), SelectedAssetPackages.Num());
	for (const FString& SelectedAssetPackage : SelectedAssetPackages)
	{
		UE_LOG(LogGFPakExporter, Verbose, TEXT("\t- %s"), *SelectedAssetPackage);
	}
	
	AddFolderMenuOptions(InMenu, SelectedPackagePaths, SelectedAssetPackages);
}

void FGFPakExporterAssetFolderContextMenu::PopulateAssetFileContextMenu(UToolMenu* InMenu)
{
}

void FGFPakExporterAssetFolderContextMenu::AddFileMenuOptions(UToolMenu* InMenu, const TArray<FName>& InSelectedPackageNames)
{
}

void FGFPakExporterAssetFolderContextMenu::AddFolderMenuOptions(UToolMenu* InMenu, const TArray<FString>& InSelectedPackagePaths, const TArray<FString>& InSelectedAssetPackages)
{
	const UContentBrowserDataMenuContext_FolderMenu* Context = InMenu->FindContext<UContentBrowserDataMenuContext_FolderMenu>();
	checkf(Context, TEXT("Required context UContentBrowserDataMenuContext_FolderMenu was missing!"));

	// const bool bCanBeModified = !Context || Context->bCanBeModified;
	if (!Context)
	{
		return;
	}

	// Check if we have right clicked on a Plugin folder
	TArray<TSharedRef<IPlugin>> Plugins = GetPluginsFromPackagePaths(InSelectedPackagePaths); //todo: ensure this is not a DLC

	if (Plugins.IsEmpty())
	{
		return;
	}
	
	
	FText ErrorMessage;
	if (Plugins.Num() > 1)
	{
		//todo: handle a cooking queue
		ErrorMessage = LOCTEXT("GFPakExporter_CreateDLC_Error_MultiplePlugins", "\nThis can only be done on one Plugin at a time");
	}
	else
	{
		UGFPakLoaderSubsystem::Get()->EnumeratePakPluginsWithStatus<UGFPakLoaderSubsystem::EComparison::GreaterOrEqual>(EGFPakLoaderStatus::Mounted,
		[&](UGFPakPlugin* PakPlugin)
		{
			if (PakPlugin->GetPluginInterface() == Plugins[0])
			{
				ErrorMessage = LOCTEXT("GFPakExporter_CreateDLC_Error_PakPlugin", "\nCannot create a DLC out of another Pak Plugin.");
				return UGFPakLoaderSubsystem::EForEachResult::Break;
			}
			return UGFPakLoaderSubsystem::EForEachResult::Continue;
		});
	}

	bool bCanCreateDLC = ErrorMessage.IsEmpty();
	if (!bCanCreateDLC)
	{
		Plugins.Empty(); // Clear references
	}
	
	FToolMenuSection& Section = InMenu->AddSection(TEXT("GFPakExporterActions"), LOCTEXT("GFPakExporterActionsMenuHeading", "Aurora"));
	Section.InsertPosition = FToolMenuInsert(TEXT("PathViewFolderOptions"), EToolMenuInsertType::Before);

	// Export Assets Action
	{
		FToolMenuEntry& Menu = Section.AddMenuEntry(
			TEXT("GFPakExporter_CreateDLC"),
			LOCTEXT("GFPakExporter_CreateDLC_MenuEntry", "Create Plugin DLC Pak"),
			FText::Format(LOCTEXT("GFPakExporter_CreateDLC_MenuEntryTooltip", "Created a cooked DLC Pak of the given Plugin.{0}"), ErrorMessage),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"),
			FUIAction(
				FExecuteAction::CreateStatic(&FGFPakExporterAssetFolderContextMenu::ExecuteCreateDLCAction, Plugins),
				FCanExecuteAction::CreateLambda([bCanCreateDLC](){ return bCanCreateDLC; })
			)
		);
	}
}

void FGFPakExporterAssetFolderContextMenu::GetSelectedAssetsInPaths(const TArray<FString>& InPaths, TArray<FName>& OutSelectedPackageNames)
{
}

void FGFPakExporterAssetFolderContextMenu::GetSelectedFilesAndFolders(const UToolMenu* InMenu, TArray<FString>& OutSelectedPackagePaths, TArray<FString>& OutSelectedAssetPackages) const
{
	UE_LOG(LogGFPakExporter, Verbose, TEXT("FGFPakExporterAssetFolderContextMenu::GetSelectedFilesAndFolders - InMenu: %s"), *GetNameSafe(InMenu));
	check(InMenu);

	const UContentBrowserDataMenuContext_FolderMenu* ContextObject = InMenu->FindContext<UContentBrowserDataMenuContext_FolderMenu>();
	checkf(ContextObject, TEXT("Required context UContentBrowserDataMenuContext_FolderMenu was missing!"));

	// Extract the internal package paths that belong to this data source from the full list of selected items given in the context
	for (const FContentBrowserItem& SelectedItem : ContextObject->SelectedItems)
	{
		const FContentBrowserItemData* SelectedItemData = SelectedItem.GetPrimaryInternalItem();
		if (!SelectedItemData)
		{
			continue;
		}

		const UContentBrowserDataSource* DataSource = SelectedItemData->GetOwnerDataSource();
		if (!DataSource)
		{
			continue;
		}

		for (const FContentBrowserItemData& InternalItems : SelectedItem.GetInternalItems())
		{
			if (const TSharedPtr<const FContentBrowserAssetFolderItemDataPayload> FolderPayload = ContentBrowserAssetData::GetAssetFolderItemPayload(DataSource, InternalItems))
			{
				OutSelectedPackagePaths.Add(FolderPayload->GetInternalPath().ToString());
			}
			else if (const TSharedPtr<const FContentBrowserAssetFileItemDataPayload> ItemPayload = ContentBrowserAssetData::GetAssetFileItemPayload(DataSource, InternalItems))
			{
				OutSelectedAssetPackages.Add(ItemPayload->GetAssetData().PackageName.ToString());
			}
		}
	}
}

void FGFPakExporterAssetFolderContextMenu::ExecuteCreateDLCAction(TArray<TSharedRef<IPlugin>> InPlugins)
{
	UE_LOG(LogGFPakExporter, Display, TEXT("FGFPakExporterAssetFolderContextMenu::ExecuteExportAction - Package names %d"), InPlugins.Num());

	if (InPlugins.Num() != 1)
	{
		return;
	}

	TSharedRef<IPlugin> Plugin = InPlugins[0];
	
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
	ILauncherProfileRef Profile = LauncherProfileManager->CreateUnsavedProfile(FString::Printf(TEXT("AuroraDLC-%s"), *Plugin->GetFriendlyName()));
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
	Profile->SetDLCName(Plugin->GetName());
	Profile->SetDLCIncludeEngineContent(true); //todo: setting
	// Cook - Advanced
	Profile->SetUnversionedCooking(false); // todo: setting
	Profile->SetDeployWithUnrealPak(true);

	FString AssetRegistryFolder = FPaths::GetPath(FGFPakExporterCommandletModule::GetTempAssetRegistryPath());
	FString CookOptions = FString::Printf(TEXT("-AuroraDLCPlugin=%s"), *Plugin->GetName());
	CookOptions += FString::Printf(TEXT(" -BasedOnReleaseVersionRoot=\"%s\""), *AssetRegistryFolder);
	CookOptions += TEXT(" -DevelopmentAssetRegistryPlatformOverride=..\\");
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
		LauncherWorker->OnOutputReceived().AddStatic(&FGFPakExporterAssetFolderContextMenu::MessageReceived);
		// Allows us to exit this command once the launcher worker has completed or is canceled
		LauncherWorker->OnStageStarted().AddStatic(&FGFPakExporterAssetFolderContextMenu::HandleStageStarted);
		LauncherWorker->OnStageCompleted().AddStatic(&FGFPakExporterAssetFolderContextMenu::HandleStageCompleted);
		LauncherWorker->OnCompleted().AddStatic(&FGFPakExporterAssetFolderContextMenu::LaunchCompleted);
		LauncherWorker->OnCanceled().AddStatic(&FGFPakExporterAssetFolderContextMenu::LaunchCanceled);

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

TArray<TSharedRef<IPlugin>> FGFPakExporterAssetFolderContextMenu::GetPluginsFromPackagePaths(TArray<FString> InPackagePaths)
{
	TArray<TSharedRef<IPlugin>> OutPlugins;

	for (FString Path : InPackagePaths)
	{
		Path.RemoveFromEnd(TEXT("/"));
	}
	
	IPluginManager& PluginManager = IPluginManager::Get();
	TArray<TSharedRef<IPlugin>> AllPlugins = PluginManager.GetDiscoveredPlugins(); // We don't care if they are enabled or not
	for (const TSharedRef<IPlugin>& Plugin : AllPlugins)
	{
		if (Plugin->CanContainContent())
		{
			FString MountedAssetPath = Plugin->GetMountedAssetPath(); // "/PluginMountPoint/"
			MountedAssetPath.RemoveFromEnd(TEXT("/")); // "/PluginMountPoint"
			if (InPackagePaths.Remove(MountedAssetPath) > 0)
			{
				OutPlugins.Add(Plugin);
				if (InPackagePaths.IsEmpty())
				{
					break;
				}
			}
		}
	}
	
	return OutPlugins;
}

void FGFPakExporterAssetFolderContextMenu::MessageReceived(const FString& InMessage)
{
	GLog->Logf(ELogVerbosity::Log, TEXT("%s"), *InMessage);
}

void FGFPakExporterAssetFolderContextMenu::HandleStageStarted(const FString& InStage)
{
	UE_LOG(LogGFPakExporter, Warning, TEXT("Starting stage %s."), *InStage);
}

void FGFPakExporterAssetFolderContextMenu::HandleStageCompleted(const FString& InStage, double StageTime)
{
	UE_LOG(LogGFPakExporter, Warning, TEXT("Completed Stage %s."), *InStage);
}

void FGFPakExporterAssetFolderContextMenu::LaunchCompleted(bool Outcome, double ExecutionTime, int32 ReturnCode)
{
	UE_LOG(LogGFPakExporter, Log, TEXT("Profile launch command %s."), Outcome ? TEXT("is SUCCESSFUL") : TEXT("has FAILED"));
}

void FGFPakExporterAssetFolderContextMenu::LaunchCanceled(double ExecutionTime)
{
	UE_LOG(LogGFPakExporter, Log, TEXT("Profile launch command was canceled."));
}

#undef LOCTEXT_NAMESPACE
