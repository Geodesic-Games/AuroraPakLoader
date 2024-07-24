// Copyright GeoTech BV


#include "GFPakExporterSubsystem.h"

#include "AuroraBuildTask.h"
#include "AuroraExporterConfig.h"
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

void UGFPakExporterSubsystem::PromptForExport(const FAuroraExporterSettings& InSettings)
{
	SAuroraExportWizard::OpenWizard(InSettings, SAuroraExportWizard::FOnExportWizardCompleted::CreateLambda(
		[](TOptional<FAuroraExporterSettings> Settings)
	{
		if (UGFPakExporterSubsystem* Subsystem = UGFPakExporterSubsystem::Get())
		{
			if (Settings)
			{
				ILauncherProfilePtr Profile = Subsystem->CreateLauncherProfileFromSettings(Settings.GetValue());
				Subsystem->LaunchProfile(Profile, Settings.GetValue());
			}
		}
	}));
}

ILauncherProfilePtr UGFPakExporterSubsystem::CreateLauncherProfileFromSettings(const FAuroraExporterSettings& InSettings) const
{
	UE_LOG(LogGFPakExporter, Display, TEXT("UGFPakExporterSubsystem::CreateLauncherProfileFromConfig"));
	
	if (!InSettings.Config.IsValid())
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("The given Config is invalid (either empty or no DLCName given)"));
		return nullptr;
	}
	
	FString ConfigFilename = InSettings.SettingsFilePath.FilePath.IsEmpty() ? FPaths::Combine(FGFPakExporterModule::GetPluginTempDir(), TEXT("AuroraExporterConfig.json")) : InSettings.SettingsFilePath.FilePath;
	if (!InSettings.SaveJsonSettings(ConfigFilename))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("Unable save the 'AuroraExporterConfig' to '%s'"), *ConfigFilename);
		return nullptr;
	}
	UE_LOG(LogGFPakExporter, Display, TEXT("Saved the Exporter Config to '%s'"), *ConfigFilename);
	
	ILauncherServicesModule& LauncherServicesModule = FModuleManager::LoadModuleChecked<ILauncherServicesModule>(TEXT("LauncherServices"));
	TSharedRef<ILauncherProfileManager> LauncherProfileManager = LauncherServicesModule.GetProfileManager();
	TArray<ITargetPlatform*> Platforms = GetTargetPlatformManager()->GetTargetPlatforms();
	ITargetDeviceServicesModule& DeviceServiceModule = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>(TEXT("TargetDeviceServices"));
	TSharedRef<ITargetDeviceProxyManager> DeviceProxyManager = DeviceServiceModule.GetDeviceProxyManager();
	
	ILauncherProfileRef Profile = LauncherProfileManager->CreateUnsavedProfile(FString::Printf(TEXT("AuroraDLC-%s"), *InSettings.Config.DLCName));
	Profile->SetDefaults();

	Profile->SetProjectSpecified(true);
	Profile->SetBuildUAT(InSettings.bBuildUAT);
	// Build
	Profile->SetBuildConfiguration(InSettings.GetBuildConfiguration());
	// Cook
	Profile->SetCookConfiguration(InSettings.GetCookConfiguration());
	Profile->SetCookMode(ELauncherProfileCookModes::ByTheBook);
	Profile->AddCookedPlatform(TEXT("Windows")); // todo: How to derive this?
	// Cook - Release DLC
	Profile->SetBasedOnReleaseVersionName(TEXT("AuroraDummyReleaseVersion"));
	Profile->SetCreateDLC(true);
	Profile->SetDLCName(InSettings.Config.DLCName);
	Profile->SetDLCIncludeEngineContent(true);
	// Cook - Advanced
	Profile->SetUnversionedCooking(InSettings.bCookUnversioned);
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
	
	return Profile;
}

TSharedPtr<FAuroraBuildTask> UGFPakExporterSubsystem::LaunchProfile(const ILauncherProfilePtr& InProfile, const FAuroraExporterSettings& InSettings)
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
	AuroraBuildTask = MakeShared<FAuroraBuildTask>(InProfile, InSettings);
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

	// We won't actually ever block the save
	return true;
}

#undef LOCTEXT_NAMESPACE
