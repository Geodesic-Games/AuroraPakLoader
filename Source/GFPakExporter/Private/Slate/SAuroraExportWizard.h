// Copyright GeoTech BV

#pragma once

#include "AuroraExporterConfig.h"
#include "Widgets/SCompoundWidget.h"

class SPrimaryButton;

/** A modal dialog to show settings to export an Aurora DLC Pak */
class SAuroraExportWizard : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnExportWizardCompleted, TOptional<FAuroraExporterSettings> /*Settings*/)

	SLATE_BEGIN_ARGS(SAuroraExportWizard) {}
	SLATE_END_ARGS()
	
	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const FAuroraExporterSettings& InInitialSettings, const FOnExportWizardCompleted& InOnExportWizardCompleted);

	/** Opens the dialog in a new window */
	static void OpenWizard(const FAuroraExporterSettings& InInitialSettings, const FOnExportWizardCompleted& InOnExportWizardCompleted);

	/** Closes the dialog */
	void CloseDialog();
private:
	FReply HandleExportButtonClicked();
	FReply HandleCancelButtonClicked();

	bool HandleIsPropertyFromConfigVisible(const FPropertyAndParent& InPropertyAndParent);
	bool HandleIsPropertyFromSettingsVisible(const FPropertyAndParent& InPropertyAndParent);
	
	/** Delegate triggered on wizard completion */
	FOnExportWizardCompleted OnWizardCompleted;

	/** Settings */
	// FAuroraExporterSettings Settings;

	//structonscope for details panel
	TSharedPtr<TStructOnScope<FAuroraExporterSettings>> Settings;
	TSharedPtr<IStructureDetailsView> ConfigDetailsView;
	TSharedPtr<IStructureDetailsView> SettingsDetailsView;
	TSharedPtr<SPrimaryButton> ExportButton;
};
