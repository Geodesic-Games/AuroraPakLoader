// Copyright GeoTech BV

#include "SAuroraExportWizard.h"

#include "GFPakExporterSubsystem.h"
#include "IStructureDetailsView.h"
#include "SPrimaryButton.h"
#include "Interfaces/IMainFrameModule.h"


#define LOCTEXT_NAMESPACE "AuroraExportWizard"


void SAuroraExportWizard::Construct(const FArguments& InArgs, const FAuroraExporterSettings& InInitialSettings, const FOnExportWizardCompleted& InOnExportWizardCompleted)
{
	OnWizardCompleted = InOnExportWizardCompleted;

	// Settings = InInitialSettings;


	Settings = MakeShared<TStructOnScope<FAuroraExporterSettings>>();
	Settings->InitializeAs<FAuroraExporterSettings>(InInitialSettings);

	FStructureDetailsViewArgs StructureViewArgs;
	StructureViewArgs.bShowObjects = true;
	StructureViewArgs.bShowAssets = true;
	StructureViewArgs.bShowClasses = true;
	StructureViewArgs.bShowInterfaces = true;

	FDetailsViewArgs ViewArgs;
	ViewArgs.bAllowSearch = false;
	ViewArgs.bShowObjectLabel = false;
	ViewArgs.ColumnWidth = 0.8f;
	ViewArgs.bShowPropertyMatrixButton = false;
	ViewArgs.bShowOptions = false;
	ViewArgs.bShowKeyablePropertiesOption = false;
	ViewArgs.bShowAnimatedPropertiesOption = false;
	ViewArgs.bCustomNameAreaLocation = true;
	ViewArgs.bShowModifiedPropertiesOption = false;

	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	ConfigDetailsView = PropertyEditor.CreateStructureDetailView(ViewArgs, StructureViewArgs, TSharedPtr<FStructOnScope>());
	{
		ConfigDetailsView->GetDetailsView()->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SAuroraExportWizard::HandleIsPropertyFromConfigVisible));
		// ConfigDetailsView->GetOnFinishedChangingPropertiesDelegate().AddSP()
		ConfigDetailsView->SetStructureData(Settings); //The Visibility will only be checked when setting the structure
	}
	SettingsDetailsView = PropertyEditor.CreateStructureDetailView(ViewArgs, StructureViewArgs, TSharedPtr<FStructOnScope>());
	{
		SettingsDetailsView->GetDetailsView()->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SAuroraExportWizard::HandleIsPropertyFromSettingsVisible));
		SettingsDetailsView->SetStructureData(Settings); //The Visibility will only be checked when setting the structure
	}
	
	ChildSlot
	[
		SNew(SBorder)
		.Padding(18)
		.BorderImage(FAppStyle::GetBrush("Docking.Tab.ContentAreaBrush"))
		[
			SNew(SVerticalBox)

			// Title
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))
				.Text(LOCTEXT("Export_Title", "Export DLC Pak"))
				.TransformPolicy(ETextTransformPolicy::ToUpper)
			]
			
			// Config Details Panel
			+SVerticalBox::Slot()
			.FillHeight(1.0)
			.Padding(0.f, 8.f, 0.f, 4.f)
			[
				ConfigDetailsView->GetWidget().ToSharedRef()
			]

			// Title
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))
				.Text(LOCTEXT("Export_Settings_Title", "Packaging Settings"))
				.TransformPolicy(ETextTransformPolicy::ToUpper)
			]
			
			// Settings
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 0.f, 4.f)
			[
				SettingsDetailsView->GetWidget().ToSharedRef()
			]
			
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0.f, 8.f, 0.f, 4.f)
			[
				SNew(SHorizontalBox)

				// Export Button
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.0f, 1.0f, 0.0f, 0.0f)
				[
					SAssignNew(ExportButton, SPrimaryButton)
					.Text(LOCTEXT("Export_Label", "Export"))
					.OnClicked(this, &SAuroraExportWizard::HandleExportButtonClicked)
					.IsEnabled(TAttribute<bool>::CreateSPLambda(this, [Settings = Settings]()
					{
						if (GEditor && Settings)
						{
							UGFPakExporterSubsystem* Subsystem = GEditor->GetEditorSubsystem<UGFPakExporterSubsystem>();
							if (Subsystem && !Subsystem->IsExporting())
							{
								if (FAuroraExporterSettings* ExporterSettings = Settings->Cast<FAuroraExporterSettings>())
								{
									return !ExporterSettings->Config.DLCName.IsEmpty();
								}
							}
						}
						return false;
					}))
				]

				// Cancel Button
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.0f, 1.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.Text(LOCTEXT("Cancel_Label", "Cancel"))
					.TextStyle(FAppStyle::Get(), "DialogButtonText")
					.OnClicked(this, &SAuroraExportWizard::HandleCancelButtonClicked)
				]
			]
		]
	];
}

void SAuroraExportWizard::OpenWizard(const FAuroraExporterSettings& InInitialSettings, const FOnExportWizardCompleted& InOnExportWizardCompleted)
{
	const TSharedRef<SWindow> ReportWindow = SNew(SWindow)
		.Title(LOCTEXT("Window_Title", "Aurora | Export Content DLC Pak"))
		.ClientSize(FVector2D(960, 700))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SAuroraExportWizard, InInitialSettings, InOnExportWizardCompleted)
		];
	

	const IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	const TSharedPtr<SWindow> ParentWindow = MainFrameModule.GetParentWindow();
	
	if (ParentWindow.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(ReportWindow, MainFrameModule.GetParentWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(ReportWindow);
	}
}

void SAuroraExportWizard::CloseDialog()
{
	const TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (ContainingWindow.IsValid())
	{
		ContainingWindow->RequestDestroyWindow();
	}
}

FReply SAuroraExportWizard::HandleExportButtonClicked()
{
	CloseDialog();
	FAuroraExporterSettings* SettingsPtr = Settings->Cast<FAuroraExporterSettings>();
	TOptional<FAuroraExporterSettings> ReturnSettings = SettingsPtr ? TOptional<FAuroraExporterSettings>(*SettingsPtr) : TOptional<FAuroraExporterSettings>{};
	OnWizardCompleted.ExecuteIfBound(ReturnSettings);
	return FReply::Handled();
}

FReply SAuroraExportWizard::HandleCancelButtonClicked()
{
	CloseDialog();
	OnWizardCompleted.ExecuteIfBound(TOptional<FAuroraExporterSettings>{});
	return FReply::Handled();
}

bool SAuroraExportWizard::HandleIsPropertyFromConfigVisible(const FPropertyAndParent& InPropertyAndParent)
{
	FName ConfigProperty = GET_MEMBER_NAME_CHECKED(FAuroraExporterSettings, Config);
	return ConfigProperty == InPropertyAndParent.Property.GetFName() ||
		InPropertyAndParent.ParentProperties.ContainsByPredicate(
			[&ConfigProperty](const FProperty* Prop) { return Prop->GetFName() == ConfigProperty; });
}

bool SAuroraExportWizard::HandleIsPropertyFromSettingsVisible(const FPropertyAndParent& InPropertyAndParent)
{
	return !HandleIsPropertyFromConfigVisible(InPropertyAndParent);
}

#undef LOCTEXT_NAMESPACE
