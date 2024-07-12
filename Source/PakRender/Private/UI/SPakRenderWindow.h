// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "SPakRenderWindow.generated.h"

class ULevelSequence;

USTRUCT()
struct PAKRENDER_API FPakSequenceData
{
	GENERATED_BODY()

	UPROPERTY()
	FString SequenceName;

	UPROPERTY()
	ULevelSequence* LevelSequence;

	UPROPERTY()
	UWorld* SequenceWorld;
};

/**
 * 
 */
class PAKRENDER_API SPakRenderWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPakRenderWindow)
		{
		}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	FReply OnLoadSequencesButtonClicked();
	FReply OnRenderSequenceButtonClicked();

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FPakSequenceData> Item, const TSharedRef<STableViewBase>& OwnerTable);

	TArray<TSharedPtr<FPakSequenceData>> SequenceDataList;
	TSharedPtr<SListView<TSharedPtr<FPakSequenceData>>> ListViewWidget;
};
