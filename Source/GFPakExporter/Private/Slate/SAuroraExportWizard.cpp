// Copyright GeoTech BV

#include "SAuroraExportWizard.h"

#include "GFPakExporterSubsystem.h"
#include "IStructureDetailsView.h"
#include "SPrimaryButton.h"
#include "Interfaces/IMainFrameModule.h"


#define LOCTEXT_NAMESPACE "AuroraExportWizard"


void SAuroraExportWizard::Construct(const FArguments& InArgs, const FAuroraContentDLCExporterSettings& InInitialDLCSettings, const FOnContentDLCExportWizardCompleted& InOnExportWizardCompleted)
{
	OnContentDLCWizardCompleted = InOnExportWizardCompleted;

	ContentDLCSettings = MakeShared<TStructOnScope<FAuroraContentDLCExporterSettings>>();
	ContentDLCSettings->InitializeAs<FAuroraContentDLCExporterSettings>(InInitialDLCSettings);
	BaseGameSettings = nullptr;
	
	Construct(InArgs);
}

void SAuroraExportWizard::Construct(const FArguments& InArgs, const FAuroraBaseGameExporterSettings& InInitialGameSettings, const FOnBaseGameExportWizardCompleted& InOnExportWizardCompleted)
{
	OnBaseGameWizardCompleted = InOnExportWizardCompleted;

	BaseGameSettings = MakeShared<TStructOnScope<FAuroraBaseGameExporterSettings>>();
	BaseGameSettings->InitializeAs<FAuroraBaseGameExporterSettings>(InInitialGameSettings);
	ContentDLCSettings = nullptr;
	
	Construct(InArgs);
}

void SAuroraExportWizard::OpenWizard(const FAuroraContentDLCExporterSettings& InInitialDLCSettings, const FOnContentDLCExportWizardCompleted& InOnExportWizardCompleted)
{
	const TSharedRef<SWindow> WizardWindow = SNew(SWindow)
		.Title(LOCTEXT("Window_Title_ContentDLC", "Aurora | Export Content DLC Pak"))
		.ClientSize(FVector2D(960, 700))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SAuroraExportWizard, InInitialDLCSettings, InOnExportWizardCompleted)
		];
	
	OpenWizard(WizardWindow);
}

void SAuroraExportWizard::OpenWizard(const FAuroraBaseGameExporterSettings& InInitialGameSettings, const FOnBaseGameExportWizardCompleted& InOnExportWizardCompleted)
{
	const TSharedRef<SWindow> WizardWindow = SNew(SWindow)
		.Title(LOCTEXT("Window_Title_BaseGame", "Aurora | Export Packaged Project"))
		.ClientSize(FVector2D(960, 700))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SAuroraExportWizard, InInitialGameSettings, InOnExportWizardCompleted)
		];
	
	OpenWizard(WizardWindow);
}

void SAuroraExportWizard::CloseDialog()
{
	const TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (ContainingWindow.IsValid())
	{
		ContainingWindow->RequestDestroyWindow();
	}
}

void SAuroraExportWizard::OpenWizard(const TSharedRef<SWindow>& WizardWindow)
{
	const IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	const TSharedPtr<SWindow> ParentWindow = MainFrameModule.GetParentWindow();
	
	if (ParentWindow.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(WizardWindow, MainFrameModule.GetParentWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(WizardWindow);
	}
}

void SAuroraExportWizard::Construct(const FArguments& InArgs)
{
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
		if (IsBaseGameExport()) //The Visibility will only be checked when setting the structure
		{
			ConfigDetailsView->SetStructureData(BaseGameSettings);
		}
		else
		{
			ConfigDetailsView->SetStructureData(ContentDLCSettings);
		}
	}
	SettingsDetailsView = PropertyEditor.CreateStructureDetailView(ViewArgs, StructureViewArgs, TSharedPtr<FStructOnScope>());
	{
		SettingsDetailsView->GetDetailsView()->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SAuroraExportWizard::HandleIsPropertyFromSettingsVisible));
		if (IsBaseGameExport()) //The Visibility will only be checked when setting the structure
		{
			SettingsDetailsView->SetStructureData(BaseGameSettings);
		}
		else
		{
			SettingsDetailsView->SetStructureData(ContentDLCSettings);
		}
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
				.Text(IsBaseGameExport() ? LOCTEXT("Export_Title_BaseGame", "Export Packaged Base Game Project") : LOCTEXT("Export_Title_ContentDLC", "Export DLC Pak"))
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
					.IsEnabled(TAttribute<bool>::CreateSPLambda(this, [bIsBaseGameExport = IsBaseGameExport(), ContentDLCSettings = ContentDLCSettings]()
					{
						if (bIsBaseGameExport)
						{
							return true;
						}
						
						if (GEditor && ContentDLCSettings)
						{
							UGFPakExporterSubsystem* Subsystem = GEditor->GetEditorSubsystem<UGFPakExporterSubsystem>();
							if (Subsystem && !Subsystem->IsExporting())
							{
								if (FAuroraContentDLCExporterSettings* ExporterSettings = ContentDLCSettings->Cast<FAuroraContentDLCExporterSettings>())
								{
									return ExporterSettings->Config.IsValid();
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

FReply SAuroraExportWizard::HandleExportButtonClicked()
{
	CloseDialog();
	if (IsBaseGameExport())
	{
		FAuroraBaseGameExporterSettings* SettingsPtr = BaseGameSettings->Cast<FAuroraBaseGameExporterSettings>();
		TOptional<FAuroraBaseGameExporterSettings> ReturnSettings = SettingsPtr ? TOptional<FAuroraBaseGameExporterSettings>(*SettingsPtr) : TOptional<FAuroraBaseGameExporterSettings>{};
		OnBaseGameWizardCompleted.ExecuteIfBound(ReturnSettings);
	}
	else
	{
		FAuroraContentDLCExporterSettings* SettingsPtr = ContentDLCSettings->Cast<FAuroraContentDLCExporterSettings>();
		TOptional<FAuroraContentDLCExporterSettings> ReturnSettings = SettingsPtr ? TOptional<FAuroraContentDLCExporterSettings>(*SettingsPtr) : TOptional<FAuroraContentDLCExporterSettings>{};
		OnContentDLCWizardCompleted.ExecuteIfBound(ReturnSettings);
	}
	
	return FReply::Handled();
}

FReply SAuroraExportWizard::HandleCancelButtonClicked()
{
	CloseDialog();
	if (IsBaseGameExport())
	{
		OnBaseGameWizardCompleted.ExecuteIfBound(TOptional<FAuroraBaseGameExporterSettings>{});
	}
	else
	{
		OnContentDLCWizardCompleted.ExecuteIfBound(TOptional<FAuroraContentDLCExporterSettings>{});
	}
	
	return FReply::Handled();
}

bool SAuroraExportWizard::HandleIsPropertyFromConfigVisible(const FPropertyAndParent& InPropertyAndParent)
{
	FName ConfigProperty = GET_MEMBER_NAME_CHECKED(FAuroraContentDLCExporterSettings, Config);
	return ConfigProperty == InPropertyAndParent.Property.GetFName() ||
		InPropertyAndParent.ParentProperties.ContainsByPredicate(
			[&ConfigProperty](const FProperty* Prop) { return Prop->GetFName() == ConfigProperty; });
}

bool SAuroraExportWizard::HandleIsPropertyFromSettingsVisible(const FPropertyAndParent& InPropertyAndParent)
{
	return !HandleIsPropertyFromConfigVisible(InPropertyAndParent);
}

#undef LOCTEXT_NAMESPACE
