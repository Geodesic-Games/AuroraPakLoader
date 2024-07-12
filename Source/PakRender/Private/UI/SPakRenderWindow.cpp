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
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "PakRender"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SPakRenderWindow::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(300)
		.HeightOverride(300)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1)
				[
					SNew(SButton)
					.Text(LOCTEXT("LoadSequencesButton", "Load Sequences"))
					.OnClicked(this, &SPakRenderWindow::OnLoadSequencesButtonClicked)
				]
				+ SVerticalBox::Slot()
				.FillHeight(1)
				[
					SNew(SButton)
					.Text(LOCTEXT("RenderSequence", "Render Sequence"))
					.OnClicked(this, &SPakRenderWindow::OnRenderSequenceButtonClicked)
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SAssignNew(ListViewWidget, SListView<TSharedPtr<FPakSequenceData>>)
				.ListItemsSource(&SequenceDataList)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SPakRenderWindow::OnGenerateRow)
			]
		]
	];
}

FReply SPakRenderWindow::OnLoadSequencesButtonClicked()
{
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
	return FReply::Handled();
}

FReply SPakRenderWindow::OnRenderSequenceButtonClicked()
{
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(ListViewWidget->GetSelectedItems()[0].Get()->LevelSequence);
	UWorld* World = ListViewWidget->GetSelectedItems()[0].Get()->SequenceWorld;

	if (!LevelSequence)
	{
		UE_LOG(LogTemp, Error, TEXT("Sequence could not be found"));
		return FReply::Handled();
	}

	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("Level could not be found"));
		return FReply::Handled();
	}
	
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

	TSubclassOf<UMoviePipelineExecutorBase> ExecutorClass = ProjectSettings->DefaultRemoteExecutor.TryLoadClass<
		UMoviePipelineExecutorBase>();
	UMoviePipelineExecutorBase* Executor = QueueSubsystem->RenderQueueWithExecutor(ExecutorClass);

	// TO DO: Close remote renderer when finished
		
	return FReply::Handled();
}

TSharedRef<ITableRow> SPakRenderWindow::OnGenerateRow(TSharedPtr<FPakSequenceData> Item,
                                                      const TSharedRef<STableViewBase>& OwnerTable)
{
	return
		SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item.Get()->SequenceName))
		];
}


END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
