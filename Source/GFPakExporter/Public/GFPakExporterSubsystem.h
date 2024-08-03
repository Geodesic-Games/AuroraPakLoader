// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"
#include "AuroraExporterSettings.h"
#include "EditorSubsystem.h"
#include "ILauncher.h"
#include "ILauncherProfile.h"
#include "GFPakExporterSubsystem.generated.h"

struct FAuroraBuildTask;
/**
 * 
 */
UCLASS(Blueprintable) //todo: add some BP functions
class GFPAKEXPORTER_API UGFPakExporterSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	static UGFPakExporterSubsystem* Get() { return GEditor ? GEditor->GetEditorSubsystem<UGFPakExporterSubsystem>() : nullptr; }

	void PromptForBaseGameExport();
	
	// DECLARE_DYNAMIC_DELEGATE_OneParam(FOnExportWizardCompleted, TOptional<FAuroraExporterSettings>, Settings)
	// todo: what delegate to create for BP? and when?
	void PromptForContentDLCExport(const FAuroraDLCExporterConfig& InConfig)
	{
		PromptForContentDLCExport(FAuroraContentDLCExporterSettings{InConfig});
	}
	void PromptForContentDLCExport(const FAuroraContentDLCExporterSettings& InSettings);

	ILauncherProfilePtr CreateBaseGameLauncherProfile(const FAuroraBaseGameExporterSettings& InBaseGameSettings) const;
	TSharedPtr<FAuroraBuildTask> LaunchBaseGameProfile(const ILauncherProfilePtr& InProfile, const FAuroraBaseGameExporterSettings& InBaseGameSettings);
	
	ILauncherProfilePtr CreateContentDLCLauncherProfileFromSettings(const FAuroraContentDLCExporterSettings& InDLCSettings) const;
	TSharedPtr<FAuroraBuildTask> LaunchContentDLCProfile(const ILauncherProfilePtr& InProfile, const FAuroraContentDLCExporterSettings& InDLCSettings);

	bool IsExporting() const;
	bool CanCloseEditor() const;
private:
	ILauncherPtr Launcher{};
	TSharedPtr<FAuroraBuildTask> AuroraBuildTask{};
	FDelegateHandle CanCloseEditorDelegate;
	FAuroraBaseGameExporterSettings LastBaseGameSettings{};
};
