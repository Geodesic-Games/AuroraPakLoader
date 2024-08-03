// Copyright GeoTech BV

#pragma once

#include "AuroraExporterSettings.h"
#include "Widgets/SCompoundWidget.h"

class SPrimaryButton;

/** A modal dialog to show settings to export an Aurora DLC Pak */
class SAuroraExportWizard : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnContentDLCExportWizardCompleted, TOptional<FAuroraContentDLCExporterSettings> /*Settings*/)
	DECLARE_DELEGATE_OneParam(FOnBaseGameExportWizardCompleted, TOptional<FAuroraBaseGameExporterSettings> /*Settings*/)

	SLATE_BEGIN_ARGS(SAuroraExportWizard) {}
	SLATE_END_ARGS()
	
	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const FAuroraContentDLCExporterSettings& InInitialDLCSettings, const FOnContentDLCExportWizardCompleted& InOnExportWizardCompleted);
	void Construct(const FArguments& InArgs, const FAuroraBaseGameExporterSettings& InInitialGameSettings, const FOnBaseGameExportWizardCompleted& InOnExportWizardCompleted);

	/** Opens the dialog in a new window */
	static void OpenWizard(const FAuroraContentDLCExporterSettings& InInitialDLCSettings, const FOnContentDLCExportWizardCompleted& InOnExportWizardCompleted);
	static void OpenWizard(const FAuroraBaseGameExporterSettings& InInitialGameSettings, const FOnBaseGameExportWizardCompleted& InOnExportWizardCompleted);

	/** Closes the dialog */
	void CloseDialog();

	bool IsBaseGameExport() const { return BaseGameSettings.IsValid(); }
	bool IsContentDLCExport() const { return ContentDLCSettings.IsValid(); }
private:
	static void OpenWizard(const TSharedRef<SWindow>& WizardWindow);
	void Construct(const FArguments& InArgs);
	
	FReply HandleExportButtonClicked();
	FReply HandleCancelButtonClicked();

	bool HandleIsPropertyFromConfigVisible(const FPropertyAndParent& InPropertyAndParent);
	bool HandleIsPropertyFromSettingsVisible(const FPropertyAndParent& InPropertyAndParent);
	
	/** Delegate triggered on wizard completion */
	FOnContentDLCExportWizardCompleted OnContentDLCWizardCompleted;
	FOnBaseGameExportWizardCompleted OnBaseGameWizardCompleted;

	//StructOnScope for details panel
	TSharedPtr<TStructOnScope<FAuroraContentDLCExporterSettings>> ContentDLCSettings;
	TSharedPtr<TStructOnScope<FAuroraBaseGameExporterSettings>> BaseGameSettings;
	TSharedPtr<IStructureDetailsView> ConfigDetailsView;
	TSharedPtr<IStructureDetailsView> SettingsDetailsView;
	TSharedPtr<SPrimaryButton> ExportButton;
};
