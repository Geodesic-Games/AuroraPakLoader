// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"
#include "AuroraExporterConfig.h"
#include "EditorSubsystem.h"
#include "ILauncher.h"
#include "ILauncherProfile.h"
#include "GFPakExporterSubsystem.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class GFPAKEXPORTER_API UGFPakExporterSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()
public:
	static UGFPakExporterSubsystem* Get() { return GEditor ? GEditor->GetEditorSubsystem<UGFPakExporterSubsystem>() : nullptr; }

	// DECLARE_DYNAMIC_DELEGATE_OneParam(FOnExportWizardCompleted, TOptional<FAuroraExporterSettings>, Settings)
	// todo: what delegate to create for BP? and when?
	void PromptForExport(const FAuroraExporterConfig& InConfig)
	{
		PromptForExport(FAuroraExporterSettings{InConfig});
	}
	void PromptForExport(const FAuroraExporterSettings& InSettings);
	
	ILauncherProfilePtr CreateLauncherProfileFromSettings(const FAuroraExporterSettings& InSettings);
	ILauncherWorkerPtr LaunchProfile(const ILauncherProfilePtr& InProfile);
private:
	void MessageReceived(const FString& InMessage, ILauncherWorkerPtr Worker);
	void HandleStageStarted(const FString& InStage, ILauncherWorkerPtr Worker);
	void HandleStageCompleted(const FString& InStage, double StageTime, ILauncherWorkerPtr Worker);
	void LaunchCompleted(bool Outcome, double ExecutionTime, int32 ReturnCode, ILauncherWorkerPtr Worker);
	void LaunchCanceled(double ExecutionTime, ILauncherWorkerPtr Worker);

	ILauncherPtr Launcher{};
	ILauncherWorkerPtr LauncherWorker{};
};
