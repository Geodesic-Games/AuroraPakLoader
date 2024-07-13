// Fill out your copyright notice in the Description page of Project Settings.


#include "SPakRenderWindow.h"
#include "GFPakLoaderSubsystem.h"
#include "LevelSequence.h"
#include "SlateOptMacros.h"
#include "LevelSequenceActor.h"
#include "MoviePipelineInProcessExecutor.h"
#include "MoviePipelineQueueEngineSubsystem.h"
#include "MoviePipelineQueueSubsystem.h"
#include "MovieRenderPipelineSettings.h"
#include "SNegativeActionButton.h"
#include "SPositiveActionButton.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "PakRender"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SPakRenderWindow::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		// Toolbar for adding pak sequences
		+ SVerticalBox::Slot()
		  .Padding(FMargin(0.f, 1.0f))
		  .AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
			.BorderBackgroundColor(FLinearColor(.6, .6, .6, 1.0f))
			[
				SNew(SHorizontalBox)

				// Add a Level Sequence to the queue
				+ SHorizontalBox::Slot()
				  .Padding(0.f, 0.f, 4.f, 0.f)
				  .VAlign(VAlign_Fill)
				  .AutoWidth()
				[
					SNew(SPositiveActionButton)
					.OnGetMenuContent(this, &SPakRenderWindow::OnGetAddSequenceMenuContent)
					.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
					.Text(LOCTEXT("AddSequence", "Add Sequence"))
				]

				// TO DO: Add spacer to prevent accidental removal?

				// Remove a Level Sequence from the queue
				+ SHorizontalBox::Slot()
				  .Padding(0.f, 0.f, 4.f, 0.f)
				  .VAlign(VAlign_Fill)
				  .AutoWidth()
				[
					SNew(SNegativeActionButton)
					.Icon(FAppStyle::Get().GetBrush("Icons.Minus"))
					.Text(LOCTEXT("RemoveSequence", "Remove Sequence"))
					.OnClicked(this, &SPakRenderWindow::RemoveSelectedJob)
				]
			]
		]

		// Main render jobs list
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(JobsListWidget, SListView<TSharedPtr<UMoviePipelineExecutorJob>>)
			.ListItemsSource(&JobsList)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SPakRenderWindow::OnGenerateJobRow)
		]

		// Footer Bar
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
			.BorderBackgroundColor(FLinearColor(.6, .6, .6, 1.0f))
			.Padding(FMargin(0, 2, 0, 2))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				  .VAlign(VAlign_Fill)
				  .HAlign(HAlign_Left)
				  .FillWidth(1.f)
				[
					SNullWidget::NullWidget
				]

				+ SHorizontalBox::Slot()
				  .Padding(0.f, 0.f, 4.f, 0.f)
				  .VAlign(VAlign_Fill)
				  .HAlign(HAlign_Right)
				  .AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("RenderSequence", "Render Sequence (Remote)"))
					.OnClicked(this, &SPakRenderWindow::OnRenderSequenceButtonClicked)
				]
			]
		]
	];
}

void SPakRenderWindow::LoadAllPakSequences()
{
	SequenceDataList.Empty();
	TArray<FAssetData> AssetDatas;

	TArray<UGFPakPlugin*> PakPlugins = UGFPakLoaderSubsystem::Get()->GetPakPlugins();
	for (UGFPakPlugin* PakPlugin : PakPlugins)
	{
		PakPlugin->GetPluginAssetsOfClass(UWorld::StaticClass(), AssetDatas);
	}

	for (FAssetData AssetData : AssetDatas)
	{
		// Not great, blocks entire editor
		UWorld* World = Cast<UWorld>(AssetData.GetAsset());
		if (World)
		{
			TArray<AActor*> OutActors;
			UGameplayStatics::GetAllActorsOfClass(World, ALevelSequenceActor::StaticClass(), OutActors);
			for (AActor* Actor : OutActors)
			{
				ALevelSequenceActor* SequenceActor = Cast<ALevelSequenceActor>(Actor);
				if (SequenceActor)
				{
					FPakSequenceData SequenceData = FPakSequenceData(SequenceActor->GetSequence()->GetPathName(),
					                                                 SequenceActor->GetSequence(), World);
					SequenceDataList.Add(MakeShared<FPakSequenceData>(SequenceData));
				}
			}
		}
	}
	ListViewWidget->RequestListRefresh();
}

FReply SPakRenderWindow::OnRenderSequenceButtonClicked()
{
	UMoviePipelineQueueEngineSubsystem* QueueSubsystem = GEngine->GetEngineSubsystem<
		UMoviePipelineQueueEngineSubsystem>();
	const UMovieRenderPipelineProjectSettings* ProjectSettings = GetDefault<UMovieRenderPipelineProjectSettings>();

	TSubclassOf<UMoviePipelineExecutorBase> ExecutorClass = ProjectSettings->DefaultRemoteExecutor.TryLoadClass<
		UMoviePipelineExecutorBase>();
	UMoviePipelineExecutorBase* Executor = QueueSubsystem->RenderQueueWithExecutor(ExecutorClass);

	// TO DO: Close remote renderer when finished

	return FReply::Handled();
}

// TO DO: fix removal
// TO DO: add transactions
FReply SPakRenderWindow::RemoveSelectedJob()
{
	JobsList.Remove(JobsListWidget->GetSelectedItems()[0]);
	UMoviePipelineQueueEngineSubsystem* QueueSubsystem = GEngine->GetEngineSubsystem<
		UMoviePipelineQueueEngineSubsystem>();
	QueueSubsystem->GetQueue()->DeleteJob(JobsListWidget->GetSelectedItems()[0].Get());

	JobsListWidget->RequestListRefresh();
	return FReply::Handled();
}

TSharedRef<SWidget> SPakRenderWindow::OnGetAddSequenceMenuContent()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("AddSequence_MenuSection", "Add Sequence to render"));
	{
		const TSharedRef<SWidget> SequencePicker = SNew(SBox)
		.WidthOverride(300)
		.HeightOverride(300)
		[
			SAssignNew(ListViewWidget, SListView<TSharedPtr<FPakSequenceData>>)
			.ListItemsSource(&SequenceDataList)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SPakRenderWindow::OnGenerateSequenceRow)
			.OnSelectionChanged(this, &SPakRenderWindow::AddSequenceToQueue)
		];
		MenuBuilder.AddWidget(SequencePicker, FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	LoadAllPakSequences();
	return MenuBuilder.MakeWidget();
}

void SPakRenderWindow::AddSequenceToQueue(TSharedPtr<FPakSequenceData> Item, ESelectInfo::Type SelectInfo)
{
	const ULevelSequence* LevelSequence = Item.Get()->LevelSequence;
	const UWorld* World = Item.Get()->SequenceWorld;

	UMoviePipelineQueueEngineSubsystem* QueueSubsystem = GEngine->GetEngineSubsystem<
		UMoviePipelineQueueEngineSubsystem>();
	UMoviePipelineQueue* Queue = QueueSubsystem->GetQueue();
	UMoviePipelineExecutorJob* Job = Queue->AllocateNewJob(UMoviePipelineInProcessExecutor::StaticClass());

	Job->Map = World;
	Job->SetSequence(LevelSequence);

	const UMovieRenderPipelineProjectSettings* ProjectSettings = GetDefault<UMovieRenderPipelineProjectSettings>();
	const TSoftObjectPtr<UMovieGraphConfig> ProjectDefaultGraph = ProjectSettings->DefaultGraph;
	if (const UMovieGraphConfig* DefaultGraph = ProjectDefaultGraph.LoadSynchronous())
	{
		Job->SetGraphPreset(DefaultGraph);
	}

	JobsList.Add(MakeShareable(Job));
	JobsListWidget->RequestListRefresh();

	FSlateApplication::Get().DismissMenuByWidget(ListViewWidget.ToSharedRef());
}

TSharedRef<ITableRow> SPakRenderWindow::OnGenerateJobRow(TSharedPtr<UMoviePipelineExecutorJob> Item,
                                                         const TSharedRef<STableViewBase>& OwnerTable)
{
	return
		SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(FText::FromString(*Item.Get()->Sequence.GetAssetName()))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(FText::FromString(*Item.Get()->Map.GetAssetName()))
			]
		];
}

TSharedRef<ITableRow> SPakRenderWindow::OnGenerateSequenceRow(TSharedPtr<FPakSequenceData> Item,
                                                              const TSharedRef<STableViewBase>& OwnerTable)
{
	return
		SNew(STableRow<TSharedPtr<FPakSequenceData>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(FText::FromString(*Item.Get()->SequenceName))
			]
		];
}


END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
