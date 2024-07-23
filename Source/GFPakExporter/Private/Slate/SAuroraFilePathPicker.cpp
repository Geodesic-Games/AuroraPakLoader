// Copyright GeoTech BV

#include "SAuroraFilePathPicker.h"
#include "DesktopPlatformModule.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"


#define LOCTEXT_NAMESPACE "SAuroraFilePathPicker"


/* SAuroraFilePathPicker interface
 *****************************************************************************/

void SAuroraFilePathPicker::Construct(const FArguments& InArgs)
{
	BrowseDirectory = InArgs._BrowseDirectory;
	BrowseTitle = InArgs._BrowseTitle;
	FilePath = InArgs._FilePath;
	FileTypeFilter = InArgs._FileTypeFilter;
	OnPathPicked = InArgs._OnPathPicked;
	DialogReturnsFullPath = InArgs._DialogReturnsFullPath;
	bIsOpenDialog = InArgs._IsOpenDialog;

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				// file path text box
				SAssignNew(TextBox, SEditableTextBox)
					.Text(this, &SAuroraFilePathPicker::HandleTextBoxText)
					.Font(InArgs._Font)
					.SelectAllTextWhenFocused(true)
					.ClearKeyboardFocusOnCommit(false)
					.OnTextCommitted(this, &SAuroraFilePathPicker::HandleTextBoxTextCommitted)
					.SelectAllTextOnCommit(false)
					.IsReadOnly(InArgs._IsReadOnly)
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				// browse button
				SNew(SButton)
					.ButtonStyle(InArgs._BrowseButtonStyle)
					.ToolTipText(InArgs._BrowseButtonToolTip)
					.OnClicked(this, &SAuroraFilePathPicker::HandleBrowseButtonClicked)
					.ContentPadding(2.0f)
					.ForegroundColor(FSlateColor::UseForeground())
					.IsFocusable(false)
					[
						SNew(SImage)
							.Image(InArgs._BrowseButtonImage)
							.ColorAndOpacity(FSlateColor::UseForeground())
					]
			]
	];
}

FReply SAuroraFilePathPicker::HandleBrowseButtonClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	if (DesktopPlatform == nullptr)
	{
		return FReply::Handled();
	}

	const FString DefaultPath = BrowseDirectory.IsSet()
		? BrowseDirectory.Get()
		: FPaths::GetPath(FilePath.Get());

	// show the file browse dialog
	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
		? ParentWindow->GetNativeWindow()->GetOSWindowHandle()
		: nullptr;

	TArray<FString> OutFiles;

	if (bIsOpenDialog.Get(true))
	{
		if (DesktopPlatform->OpenFileDialog(ParentWindowHandle, BrowseTitle.Get().ToString(), DefaultPath, TEXT(""), FileTypeFilter.Get(), EFileDialogFlags::None, OutFiles))
		{
			if (DialogReturnsFullPath.Get())
			{
				OnPathPicked.ExecuteIfBound(FPaths::ConvertRelativePathToFull(OutFiles[0]));
			}
			else
			{
				OnPathPicked.ExecuteIfBound(OutFiles[0]);
			}
		}
	}
	else
	{
		if (DesktopPlatform->SaveFileDialog(ParentWindowHandle, BrowseTitle.Get().ToString(), DefaultPath, TEXT(""), FileTypeFilter.Get(), EFileDialogFlags::None, OutFiles))
		{
			if (DialogReturnsFullPath.Get())
			{
				OnPathPicked.ExecuteIfBound(FPaths::ConvertRelativePathToFull(OutFiles[0]));
			}
			else
			{
				OnPathPicked.ExecuteIfBound(OutFiles[0]);
			}
		}
	}

	return FReply::Handled();
}

FText SAuroraFilePathPicker::HandleTextBoxText() const
{
	return FText::FromString(FilePath.Get());
}

void SAuroraFilePathPicker::HandleTextBoxTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	OnPathPicked.ExecuteIfBound(NewText.ToString());
}

#undef LOCTEXT_NAMESPACE