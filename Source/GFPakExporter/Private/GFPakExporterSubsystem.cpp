// Copyright GeoTech BV


#include "GFPakExporterSubsystem.h"

#include "AuroraExporterConfig.h"
#include "GFPakExporter.h"
#include "GFPakExporterLog.h"
#include "ILauncherProfileManager.h"
#include "ILauncherServicesModule.h"
#include "ITargetDeviceServicesModule.h"
#include "Slate/SAuroraExportWizard.h"


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
				Subsystem->LaunchProfile(Profile);
			}
		}
	}));
}

ILauncherProfilePtr UGFPakExporterSubsystem::CreateLauncherProfileFromSettings(const FAuroraExporterSettings& InSettings)
{
	UE_LOG(LogGFPakExporter, Display, TEXT("UGFPakExporterSubsystem::CreateLauncherProfileFromConfig"));
	
	if (!InSettings.Config.IsValid())
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("The given Config is invalid (either empty or no DLCName given)"));
		return nullptr;
	}
	
	FString ConfigFilename = InSettings.ConfigFilePath.FilePath.IsEmpty() ? FPaths::Combine(FGFPakExporterModule::GetPluginTempDir(), TEXT("AuroraExporterConfig.json")) : InSettings.ConfigFilePath.FilePath;
	if (!InSettings.Config.SaveJsonConfig(ConfigFilename))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("Unable save the 'AuroraExporterConfig' to '%s'"), *ConfigFilename);
		return nullptr;
	}
	UE_LOG(LogGFPakExporter, Display, TEXT("Saved the Exporter Config to '%s'"), *ConfigFilename);
	
	ILauncherServicesModule* LauncherServicesModule = FModuleManager::GetModulePtr<ILauncherServicesModule>(TEXT("LauncherServices"));
	if (!LauncherServicesModule)
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("Unable to get the 'LauncherServices' module"));
		return nullptr;
	}
	
	TSharedRef<ILauncherProfileManager> LauncherProfileManager = LauncherServicesModule->GetProfileManager();
	TArray<ITargetPlatform*> Platforms = GetTargetPlatformManager()->GetTargetPlatforms();
	ITargetDeviceServicesModule& DeviceServiceModule = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>(TEXT("TargetDeviceServices"));
	TSharedRef<ITargetDeviceProxyManager> DeviceProxyManager = DeviceServiceModule.GetDeviceProxyManager();
	
	ILauncherProfileRef Profile = LauncherProfileManager->CreateUnsavedProfile(FString::Printf(TEXT("AuroraDLC-%s"), *InSettings.Config.DLCName));
	Profile->SetDefaults();

	Profile->SetProjectSpecified(true);
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

ILauncherWorkerPtr UGFPakExporterSubsystem::LaunchProfile(const ILauncherProfilePtr& InProfile)
{
	UE_LOG(LogGFPakExporter, Verbose, TEXT("UGFPakExporterSubsystem::LaunchProfile"));
	
	if (!InProfile)
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("Launcher profile is null"));
		return nullptr;
	}
	
	if (!InProfile->IsValidForLaunch())
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("Launcher profile '%s' for is not valid for launch."),
			*InProfile->GetName());
		for (int32 i = 0; i < (int32)ELauncherProfileValidationErrors::Count; ++i)
		{
			ELauncherProfileValidationErrors::Type Error = (ELauncherProfileValidationErrors::Type)i;
			if (InProfile->HasValidationError(Error))
			{
				UE_LOG(LogGFPakExporter, Error, TEXT("ValidationError: %s"), *LexToStringLocalized(Error));
			}
		}
		return nullptr;
	}
	
	if (LauncherWorker)
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("A Profile is already processing. Unable to start a new one."));
		return nullptr;
	}

	ILauncherServicesModule* LauncherServicesModule = FModuleManager::GetModulePtr<ILauncherServicesModule>(TEXT("LauncherServices"));
	if (!LauncherServicesModule)
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("Unable to get the 'LauncherServices' module"));
		return nullptr;
	}
	
	// We are now ready, let's Launch
	if (!Launcher)
	{
		Launcher = LauncherServicesModule->CreateLauncher();
	}
	
	ITargetDeviceServicesModule& DeviceServiceModule = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>(TEXT("TargetDeviceServices"));
	TSharedRef<ITargetDeviceProxyManager> DeviceProxyManager = DeviceServiceModule.GetDeviceProxyManager();
	
	LauncherWorker = Launcher->Launch(DeviceProxyManager, InProfile.ToSharedRef());

	// This will allow us to pipe the launcher messages into the command window.
	LauncherWorker->OnOutputReceived().AddUObject(this, &UGFPakExporterSubsystem::MessageReceived, LauncherWorker);
	// Allows us to exit this command once the launcher worker has completed or is canceled
	LauncherWorker->OnStageStarted().AddUObject(this, &UGFPakExporterSubsystem::HandleStageStarted, LauncherWorker);
	LauncherWorker->OnStageCompleted().AddUObject(this, &UGFPakExporterSubsystem::HandleStageCompleted, LauncherWorker);
	LauncherWorker->OnCompleted().AddUObject(this, &UGFPakExporterSubsystem::LaunchCompleted, LauncherWorker);
	LauncherWorker->OnCanceled().AddUObject(this, &UGFPakExporterSubsystem::LaunchCanceled, LauncherWorker);

	TArray<ILauncherTaskPtr> TaskList;
	int32 NumOfTasks = LauncherWorker->GetTasks(TaskList);	
	UE_LOG(LogGFPakExporter, Display, TEXT("There are '%i' tasks to be completed."), NumOfTasks);

	return LauncherWorker;
}

void UGFPakExporterSubsystem::MessageReceived(const FString& InMessage, ILauncherWorkerPtr Worker)
{
	GLog->Logf(ELogVerbosity::Log, TEXT("%s"), *InMessage);
}

void UGFPakExporterSubsystem::HandleStageStarted(const FString& InStage, ILauncherWorkerPtr Worker)
{
	UE_LOG(LogGFPakExporter, Warning, TEXT("Starting stage %s."), *InStage);
}

void UGFPakExporterSubsystem::HandleStageCompleted(const FString& InStage, double StageTime, ILauncherWorkerPtr Worker)
{
	UE_LOG(LogGFPakExporter, Warning, TEXT("Completed Stage %s."), *InStage);
}

void UGFPakExporterSubsystem::LaunchCompleted(bool Outcome, double ExecutionTime, int32 ReturnCode, ILauncherWorkerPtr Worker)
{
	UE_LOG(LogGFPakExporter, Log, TEXT("Profile launch command %s."), Outcome ? TEXT("is SUCCESSFUL") : TEXT("has FAILED"));

	if (LauncherWorker == Worker)
	{
		LauncherWorker = nullptr;
	}
}

void UGFPakExporterSubsystem::LaunchCanceled(double ExecutionTime, ILauncherWorkerPtr Worker)
{
	UE_LOG(LogGFPakExporter, Log, TEXT("Profile launch command was canceled."));

	if (LauncherWorker == Worker)
	{
		LauncherWorker = nullptr;
	}
}
