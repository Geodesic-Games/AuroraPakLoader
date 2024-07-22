// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"
#include "AuroraExporterConfig.h"
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
	static UGFPakExporterSubsystem* Get() { return GEditor ? GEditor->GetEditorSubsystem<UGFPakExporterSubsystem>() : nullptr; }

	// DECLARE_DYNAMIC_DELEGATE_OneParam(FOnExportWizardCompleted, TOptional<FAuroraExporterSettings>, Settings)
	// todo: what delegate to create for BP? and when?
	void PromptForExport(const FAuroraExporterConfig& InConfig)
	{
		PromptForExport(FAuroraExporterSettings{InConfig});
	}
	void PromptForExport(const FAuroraExporterSettings& InSettings);
	
	ILauncherProfilePtr CreateLauncherProfileFromSettings(const FAuroraExporterSettings& InSettings) const;
	TSharedPtr<FAuroraBuildTask> LaunchProfile(const ILauncherProfilePtr& InProfile, const FAuroraExporterSettings& InSettings);

	bool IsExporting() const;
	
private:
	ILauncherPtr Launcher{};
	TSharedPtr<FAuroraBuildTask> AuroraBuildTask{};
};
