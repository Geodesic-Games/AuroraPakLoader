// Copyright GeoTech BV


#include "GFPakExporterSubsystem.h"

#include "AuroraBuildTask.h"
#include "AuroraExporterSettings.h"
#include "GFPakExporter.h"
#include "GFPakExporterLog.h"
#include "ILauncherProfileManager.h"
#include "ILauncherServicesModule.h"
#include "ITargetDeviceServicesModule.h"
#include "Interfaces/IMainFrameModule.h"
#include "Slate/SAuroraExportWizard.h"

#define LOCTEXT_NAMESPACE "UGFPakExporterSubsystem"

void UGFPakExporterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	// In editor, an asset re-save dialog can prevent AJA from cleaning up in the regular PreExit callback,
	// So we have to do our cleanup before the regular callback is called.
	IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	CanCloseEditorDelegate = MainFrame.RegisterCanCloseEditor(IMainFrameModule::FMainFrameCanCloseEditor::CreateUObject(this, &UGFPakExporterSubsystem::CanCloseEditor));

	// Load last settings
	FString DefaultBaseGameSettingsPath = GetDefaultBaseGameExporterSettingsPath();
	if (FPaths::FileExists(DefaultBaseGameSettingsPath))
	{
		LastBaseGameSettings.LoadJsonSettings(DefaultBaseGameSettingsPath);
	}
}

void UGFPakExporterSubsystem::Deinitialize()
{
	if (IsExporting())
	{
		AuroraBuildTask->Cancel();
	}
	if (IMainFrameModule* MainFrame = FModuleManager::GetModulePtr<IMainFrameModule>("MainFrame"))
	{
		MainFrame->UnregisterCanCloseEditor(CanCloseEditorDelegate);
	}
	Super::Deinitialize();
}

void UGFPakExporterSubsystem::PromptForBaseGameExport()
{
	SAuroraExportWizard::OpenWizard(LastBaseGameSettings, SAuroraExportWizard::FOnBaseGameExportWizardCompleted::CreateLambda(
		[](TOptional<FAuroraBaseGameExporterSettings> Settings)
	{
		if (UGFPakExporterSubsystem* Subsystem = UGFPakExporterSubsystem::Get())
		{
			if (Settings)
			{
				Subsystem->LastBaseGameSettings = Settings.GetValue();
				ILauncherProfilePtr Profile = Subsystem->CreateBaseGameLauncherProfile(Subsystem->LastBaseGameSettings);
				Subsystem->LaunchBaseGameProfile(Profile, Subsystem->LastBaseGameSettings);
			}
		}
	}));
}

void UGFPakExporterSubsystem::PromptForContentDLCExport(const FAuroraContentDLCExporterSettings& InSettings)
{
	SAuroraExportWizard::OpenWizard(InSettings, SAuroraExportWizard::FOnContentDLCExportWizardCompleted::CreateLambda(
		[](TOptional<FAuroraContentDLCExporterSettings> Settings)
	{
		if (UGFPakExporterSubsystem* Subsystem = UGFPakExporterSubsystem::Get())
		{
			if (Settings)
			{
				ILauncherProfilePtr Profile = Subsystem->CreateContentDLCLauncherProfileFromSettings(Settings.GetValue());
				Subsystem->LaunchContentDLCProfile(Profile, Settings.GetValue());
			}
		}
	}));
}

ILauncherProfilePtr UGFPakExporterSubsystem::CreateBaseGameLauncherProfile(const FAuroraBaseGameExporterSettings& InBaseGameSettings) const
{
	UE_LOG(LogGFPakExporter, Display, TEXT("UGFPakExporterSubsystem::CreateBaseGameLauncherProfile"));
	
	FString SettingsFilename = InBaseGameSettings.SettingsFilePath.FilePath.IsEmpty() ? GetDefaultBaseGameExporterSettingsPath() : InBaseGameSettings.SettingsFilePath.FilePath;
	if (!InBaseGameSettings.SaveJsonSettings(SettingsFilename))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("Unable save the 'AuroraBaseGameExporterSettings' to '%s'"), *SettingsFilename);
		return nullptr;
	}
	SettingsFilename = FPaths::ConvertRelativePathToFull(SettingsFilename);
	UE_LOG(LogGFPakExporter, Display, TEXT("Saved the Exporter Config to '%s'"), *SettingsFilename);
	
	ILauncherServicesModule& LauncherServicesModule = FModuleManager::LoadModuleChecked<ILauncherServicesModule>(TEXT("LauncherServices"));
	TSharedRef<ILauncherProfileManager> LauncherProfileManager = LauncherServicesModule.GetProfileManager();
	TArray<ITargetPlatform*> Platforms = GetTargetPlatformManager()->GetTargetPlatforms();
	ITargetDeviceServicesModule& DeviceServiceModule = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>(TEXT("TargetDeviceServices"));
	TSharedRef<ITargetDeviceProxyManager> DeviceProxyManager = DeviceServiceModule.GetDeviceProxyManager();
	
	ILauncherProfileRef Profile = LauncherProfileManager->CreateUnsavedProfile(TEXT("Aurora-BaseGame"));
	Profile->SetDefaults();

	Profile->SetProjectSpecified(true);
	Profile->SetBuildUAT(InBaseGameSettings.BuildSettings.bBuildUAT);
	// Build
	Profile->SetBuildConfiguration(InBaseGameSettings.BuildSettings.GetBuildConfiguration());
	// Cook
	Profile->SetCookConfiguration(InBaseGameSettings.BuildSettings.GetCookConfiguration());
	Profile->SetCookMode(ELauncherProfileCookModes::ByTheBook);
	Profile->AddCookedPlatform(InBaseGameSettings.BuildSettings.CookingPlatform);
	// Cook - Release DLC
	Profile->SetCreateDLC(false);
	// Cook - Advanced
	Profile->SetUnversionedCooking(InBaseGameSettings.BuildSettings.bCookUnversioned);
	Profile->SetDeployWithUnrealPak(true);
	
	Profile->SetUseZenStore(false);

	FString AssetRegistryFolder = FGFPakExporterModule::GetTempAssetRegistryDir();
	FString CookOptions = FString::Printf(TEXT("-%s=\"%s\""), *FGFPakExporterModule::AuroraBaseGameCommandLineParameter, *SettingsFilename);
	// In Project::GetGenericCookCommandletParams (in CookCommand.Automation.cs), the AdditionalCookerOptions has quotes trimmed from start and end:
	// Params.AdditionalCookerOptions.TrimStart(new char[] { '\"', ' ' }).TrimEnd(new char[] { '\"', ' ' });
	// which ends up removing the last quote if we do not add a parameter that does not need any.
	// To avoid that, we add a dummy parameter to avoid this
	Profile->SetCookOptions(CookOptions + TEXT(" -AuroraDummy"));
	
	// Package
	Profile->SetPackagingMode(ELauncherProfilePackagingModes::Locally);
	Profile->SetPackageDirectory(InBaseGameSettings.BuildSettings.PackageDirectory.Path);
	// Deploy
	Profile->SetDeploymentMode(ELauncherProfileDeploymentModes::DoNotDeploy);
	
	return Profile;
}

TSharedPtr<FAuroraBuildTask> UGFPakExporterSubsystem::LaunchBaseGameProfile(const ILauncherProfilePtr& InProfile, const FAuroraBaseGameExporterSettings& InBaseGameSettings)
{
	if (IsExporting())
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("A Build Task is already processing, unable to start a new one."));
		return nullptr;
	}
	
	if (!Launcher)
	{
		ILauncherServicesModule& LauncherServicesModule = FModuleManager::LoadModuleChecked<ILauncherServicesModule>(TEXT("LauncherServices"));
		Launcher = LauncherServicesModule.CreateLauncher();
	}
	AuroraBuildTask = MakeShared<FAuroraBuildTask>(InProfile, InBaseGameSettings);
	if (!AuroraBuildTask->Launch(Launcher))
	{
		return nullptr;
	}
	return AuroraBuildTask;
}

ILauncherProfilePtr UGFPakExporterSubsystem::CreateContentDLCLauncherProfileFromSettings(const FAuroraContentDLCExporterSettings& InDLCSettings) const
{
	UE_LOG(LogGFPakExporter, Display, TEXT("UGFPakExporterSubsystem::CreateLauncherProfileFromConfig"));
	
	if (!InDLCSettings.Config.IsValid())
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("The given Config is invalid (either empty or no DLCName given)"));
		return nullptr;
	}
	
	FString SettingsFilename = InDLCSettings.SettingsFilePath.FilePath.IsEmpty() ? GetDefaultContentDLCExporterSettingsPath() : InDLCSettings.SettingsFilePath.FilePath;
	if (!InDLCSettings.SaveJsonSettings(SettingsFilename))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("Unable save the 'AuroraContentDLCExporterSettings' to '%s'"), *SettingsFilename);
		return nullptr;
	}
	UE_LOG(LogGFPakExporter, Display, TEXT("Saved the Exporter Config to '%s'"), *SettingsFilename);
	
	ILauncherServicesModule& LauncherServicesModule = FModuleManager::LoadModuleChecked<ILauncherServicesModule>(TEXT("LauncherServices"));
	TSharedRef<ILauncherProfileManager> LauncherProfileManager = LauncherServicesModule.GetProfileManager();
	TArray<ITargetPlatform*> Platforms = GetTargetPlatformManager()->GetTargetPlatforms();
	ITargetDeviceServicesModule& DeviceServiceModule = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>(TEXT("TargetDeviceServices"));
	TSharedRef<ITargetDeviceProxyManager> DeviceProxyManager = DeviceServiceModule.GetDeviceProxyManager();
	
	ILauncherProfileRef Profile = LauncherProfileManager->CreateUnsavedProfile(FString::Printf(TEXT("AuroraDLC-%s"), *InDLCSettings.Config.DLCName));
	Profile->SetDefaults();

	Profile->SetProjectSpecified(true);
	Profile->SetBuildUAT(InDLCSettings.BuildSettings.bBuildUAT);
	// Build
	Profile->SetBuildConfiguration(InDLCSettings.BuildSettings.GetBuildConfiguration());
	// Cook
	Profile->SetCookConfiguration(InDLCSettings.BuildSettings.GetCookConfiguration());
	Profile->SetCookMode(ELauncherProfileCookModes::ByTheBook);
	Profile->AddCookedPlatform(InDLCSettings.BuildSettings.CookingPlatform);
	// Cook - Release DLC
	Profile->SetBasedOnReleaseVersionName(TEXT("AuroraDummyReleaseVersion")); // We need to give a BaseRelease version, but it will be ignored
	Profile->SetCreateDLC(true);
	Profile->SetDLCName(InDLCSettings.Config.DLCName);
	Profile->SetDLCIncludeEngineContent(true);
	// Cook - Advanced
	Profile->SetUnversionedCooking(InDLCSettings.BuildSettings.bCookUnversioned);
	Profile->SetDeployWithUnrealPak(true);
	
	Profile->SetUseZenStore(false);

	FString AssetRegistryFolder = FGFPakExporterModule::GetTempAssetRegistryDir();
	FString CookOptions = FString::Printf(TEXT("-%s=\"%s\""), *FGFPakExporterModule::AuroraContentDLCCommandLineParameter, *SettingsFilename);
	CookOptions += FString::Printf(TEXT(" -BasedOnReleaseVersionRoot=\"%s\""), *AssetRegistryFolder); // Folder containing our custom Asset Registry
	CookOptions += TEXT(" -DevelopmentAssetRegistryPlatformOverride=..\\"); // Need to be the parent folder to disregard the BasedOnReleaseVersion
	CookOptions += TEXT(" -CookSkipRequests -CookSkipSoftRefs -CookSkipHardRefs -DisableUnsolicitedPackages -NoGameAlwaysCookPackages -SkipZenStore");
	// In Project::GetGenericCookCommandletParams (in CookCommand.Automation.cs), the AdditionalCookerOptions has quotes trimmed from start and end:
	// Params.AdditionalCookerOptions.TrimStart(new char[] { '\"', ' ' }).TrimEnd(new char[] { '\"', ' ' });
	// which ends up removing the last quote if we do not add a parameter that does not need any.
	// To avoid that, we add a dummy parameter to avoid this
	Profile->SetCookOptions(CookOptions + TEXT(" -AuroraDummy"));
	
	// Package
	Profile->SetPackagingMode(ELauncherProfilePackagingModes::Locally);

	// Deploy
	Profile->SetDeploymentMode(ELauncherProfileDeploymentModes::DoNotDeploy);

	
	if (!InDLCSettings.BuildSettings.PackageDirectory.Path.IsEmpty())
	{
		Profile->SetPackageDirectory(FGFPakExporterModule::GetTempStagingDir());
		// Here sneakily adding another command to the command line as there is no other way using a Launcher Profile
		FString PlatformCookDir = FPaths::Combine(FGFPakExporterModule::GetTempCookDir(), InDLCSettings.BuildSettings.CookingPlatform); // Platform needs to be included in the path
		Profile->SetAdditionalCommandLineParameters(Profile->GetAdditionalCommandLineParameters() + FString::Printf(TEXT("\" -CookOutputDir=\"%s\" -AuroraDummy=\""), *PlatformCookDir));
		// We do not do an Archive as it copied too many folder levels compared to what we need
	}
	
	return Profile;
}

TSharedPtr<FAuroraBuildTask> UGFPakExporterSubsystem::LaunchContentDLCProfile(const ILauncherProfilePtr& InProfile, const FAuroraContentDLCExporterSettings& InDLCSettings)
{
	if (IsExporting())
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("A Build Task is already processing, unable to start a new one."));
		return nullptr;
	}
	
	if (!Launcher)
	{
		ILauncherServicesModule& LauncherServicesModule = FModuleManager::LoadModuleChecked<ILauncherServicesModule>(TEXT("LauncherServices"));
		Launcher = LauncherServicesModule.CreateLauncher();
	}
	AuroraBuildTask = MakeShared<FAuroraBuildTask>(InProfile, InDLCSettings);
	if (!AuroraBuildTask->Launch(Launcher))
	{
		return nullptr;
	}
	return AuroraBuildTask;
}

bool UGFPakExporterSubsystem::IsExporting() const
{
	return AuroraBuildTask && AuroraBuildTask->GetStatus() == ELauncherTaskStatus::Busy;
}

bool UGFPakExporterSubsystem::CanCloseEditor() const
{
	if (IsExporting())
	{
		FString DLCName = AuroraBuildTask ? AuroraBuildTask->GetProfile()->GetDLCName() : FString{};
		const EAppReturnType::Type DlgResult = FMessageDialog::Open(EAppMsgType::YesNo,
			FText::Format(LOCTEXT("ClosingEditorWhileExporting_Text", "Aurora is currently exporting the DLC '{0}'.\nDo you really want to quit and cancel the export?"), FText::FromString(DLCName)),
			LOCTEXT("ClosingEditorWhileExporting_Title", "Aurora | Cancel Export?")
		);
		return DlgResult == EAppReturnType::Yes;
	}
	
	return true;
}

FString UGFPakExporterSubsystem::GetDefaultBaseGameExporterSettingsPath()
{
	return FPaths::Combine(FGFPakExporterModule::GetPluginTempDir(), TEXT("AuroraBaseGameExporterSettings.json"));
}

FString UGFPakExporterSubsystem::GetDefaultContentDLCExporterSettingsPath()
{
	return FPaths::Combine(FGFPakExporterModule::GetPluginTempDir(), TEXT("AuroraContentDLCExporterSettings.json"));
}

#undef LOCTEXT_NAMESPACE
