// Copyright GeoTech BV
#include "AuroraBuildTask.h"

#include "GFPakExporterLog.h"
#include "ITargetDeviceServicesModule.h"
#include "OutputLogModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FGFPakLoaderEditorModule"

class FMainFrameActionsNotificationTask // As per UATHelperModule.cpp
{
public:

	FMainFrameActionsNotificationTask(const TWeakPtr<SNotificationItem>& InNotificationItemPtr, SNotificationItem::ECompletionState InCompletionState, const FText& InText, const FText& InLinkText = FText(), bool InExpireAndFadeout = true)
		: CompletionState(InCompletionState)
		, NotificationItemPtr(InNotificationItemPtr)
		, Text(InText)
		, LinkText(InLinkText)
		, bExpireAndFadeout(InExpireAndFadeout)

	{ }

	static void HandleHyperlinkNavigate()
	{
		FMessageLog("PackagingResults").Open(EMessageSeverity::Error, true);
	}

	static void HandleDismissButtonClicked()
	{
		TSharedPtr<SNotificationItem> NotificationItem = ExpireNotificationItemPtr.Pin();
		if (NotificationItem.IsValid())
		{

			NotificationItem->SetExpireDuration(0.0f);
			NotificationItem->SetFadeOutDuration(0.0f);
			NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
			NotificationItem->ExpireAndFadeout();
			ExpireNotificationItemPtr.Reset();
		}
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		if ( NotificationItemPtr.IsValid() )
		{
			if (CompletionState == SNotificationItem::CS_Fail)
			{
				GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileFailed_Cue.CompileFailed_Cue"));
			}
			else
			{
				GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileSuccess_Cue.CompileSuccess_Cue"));
			}
			
			TSharedPtr<SNotificationItem> NotificationItem = NotificationItemPtr.Pin();
			NotificationItem->SetText(Text);			

			if (!LinkText.IsEmpty())
			{
				FText VLinkText(LinkText);
				const TAttribute<FText> Message = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda([VLinkText]()
				{
					return VLinkText;
				}));

				NotificationItem->SetHyperlink(FSimpleDelegate::CreateStatic(&HandleHyperlinkNavigate), Message);

			}
			
			ExpireNotificationItemPtr = NotificationItem;
			if (bExpireAndFadeout)
			{
				NotificationItem->SetExpireDuration(6.0f);
				NotificationItem->SetFadeOutDuration(0.5f);
				NotificationItem->SetCompletionState(CompletionState);
				NotificationItem->ExpireAndFadeout();
			}
			else
			{
				// Handling the notification expiration in callback
				NotificationItem->SetCompletionState(CompletionState);
			}

			// // Since the task was probably fairly long, we should try and grab the users attention if they have the option enabled.
			// const UEditorPerProjectUserSettings* SettingsPtr = GetDefault<UEditorPerProjectUserSettings>();
			// if (SettingsPtr->bGetAttentionOnUATCompletion)
			// {
			// 	IMainFrameModule* MainFrame = FModuleManager::LoadModulePtr<IMainFrameModule>("MainFrame");
			// 	if (MainFrame != nullptr)
			// 	{
			// 		TSharedPtr<SWindow> ParentWindow = MainFrame->GetParentWindow();
			// 		if (ParentWindow != nullptr)
			// 		{
			// 			ParentWindow->DrawAttention(FWindowDrawAttentionParameters(EWindowDrawAttentionRequestType::UntilActivated));
			// 		}
			// 	}
			// }
		}
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::GameThread; }
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FMainFrameActionsNotificationTask, STATGROUP_TaskGraphTasks);
	}

private:

	static TWeakPtr<SNotificationItem> ExpireNotificationItemPtr;

	SNotificationItem::ECompletionState CompletionState;
	TWeakPtr<SNotificationItem> NotificationItemPtr;
	FText Text;
	FText LinkText;
	bool bExpireAndFadeout;
};

TWeakPtr<SNotificationItem> FMainFrameActionsNotificationTask::ExpireNotificationItemPtr;



bool FAuroraBuildTask::Launch(const ILauncherPtr& Launcher)
{
	UE_LOG(LogGFPakExporter, Verbose, TEXT("FAuroraBuildTask::Launch"));

	if (Status != ELauncherTaskStatus::Pending)
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("Aurora Build Task was already launched"));
		return false;
	}
	
	if (!ensure(Profile))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("Launcher Profile is null"));
		return false;
	}
	
	if (!Profile->IsValidForLaunch())
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("Launcher profile '%s' for is not valid for launch."),
			*Profile->GetName());
		for (int32 i = 0; i < (int32)ELauncherProfileValidationErrors::Count; ++i)
		{
			ELauncherProfileValidationErrors::Type Error = (ELauncherProfileValidationErrors::Type)i;
			if (Profile->HasValidationError(Error))
			{
				UE_LOG(LogGFPakExporter, Error, TEXT("ValidationError: %s"), *LexToStringLocalized(Error));
			}
		}
		return false;
	}
	
	if (!Launcher)
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("The given Launcher is null"));
		return false;
	}

	ITargetDeviceServicesModule& DeviceServiceModule = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>(TEXT("TargetDeviceServices"));
	TSharedRef<ITargetDeviceProxyManager> DeviceProxyManager = DeviceServiceModule.GetDeviceProxyManager();

	Status = ELauncherTaskStatus::Busy;
	LauncherWorker = Launcher->Launch(DeviceProxyManager, Profile.ToSharedRef());
	// Not ideal but not able to set callbacks before
	// This will allow us to pipe the launcher messages into the command window.
	LauncherWorker->OnOutputReceived().AddSP(this, &FAuroraBuildTask::MessageReceived);
	// Allows us to exit this command once the launcher worker has completed or is canceled
	LauncherWorker->OnStageStarted().AddSP(this, &FAuroraBuildTask::HandleStageStarted);
	LauncherWorker->OnStageCompleted().AddSP(this, &FAuroraBuildTask::HandleStageCompleted);
	LauncherWorker->OnCompleted().AddSP(this, &FAuroraBuildTask::LaunchCompleted);
	LauncherWorker->OnCanceled().AddSP(this, &FAuroraBuildTask::LaunchCanceled);

	TArray<ILauncherTaskPtr> TaskList;
	int NbTasks = LauncherWorker->GetTasks(TaskList);	
	UE_LOG(LogGFPakExporter, Display, TEXT("There are '%i' tasks to be completed."), NbTasks);

	GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileStart_Cue.CompileStart_Cue"));

	CreateNotification();
	
	return true;
}

void FAuroraBuildTask::Cancel()
{
	UE_LOG(LogGFPakExporter, Warning, TEXT("Cancelling"));
	if (LauncherWorker && Status == ELauncherWorkerStatus::Busy)
	{
		LauncherWorker->Cancel();
	}
}

void FAuroraBuildTask::MessageReceived(const FString& InMessage)
{
	GLog->Logf(ELogVerbosity::Log, TEXT("%s"), *InMessage);
}

void FAuroraBuildTask::HandleStageStarted(const FString& InStage)
{
	UE_LOG(LogGFPakExporter, Warning, TEXT("Starting stage %s."), *InStage);
	if (ensure(LauncherWorker))
	{
		TArray<ILauncherTaskPtr> TaskList;
		const int NbTasks = LauncherWorker->GetTasks(TaskList);
		const int CurrentTask = 1 + TaskList.IndexOfByPredicate([](const ILauncherTaskPtr& Task)
		{
			return Task->GetStatus() != ELauncherTaskStatus::Completed;
		});
		
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("TaskStatusNb"), FText::Format(LOCTEXT("Notification_TaskStatusNb", "[{0}/{1}] "), FText::AsNumber(CurrentTask), FText::AsNumber(NbTasks)));
		Arguments.Add(TEXT("Stage"), FText::FromString(InStage));
		
		FScopeLock Lock(&NotificationTextMutex);
		NotificationText = FText::Format(LOCTEXT("Notification_DisplayText", "{TaskStatusNb}{Stage}"), Arguments);
	}
}

void FAuroraBuildTask::HandleStageCompleted(const FString& InStage, double StageTime)
{
	UE_LOG(LogGFPakExporter, Warning, TEXT("Completed Stage %s."), *InStage);
}

void FAuroraBuildTask::LaunchCompleted(bool Outcome, double ExecutionTime, int32 ReturnCode)
{
	UE_LOG(LogGFPakExporter, Log, TEXT("Profile launch command %s."), Outcome ? TEXT("is SUCCESSFUL") : TEXT("has FAILED"));

	if (LauncherWorker)
	{
		if ( ReturnCode == 0 )
		{
			TGraphTask<FMainFrameActionsNotificationTask>::CreateTask().ConstructAndDispatchWhenReady(
				NotificationItemPtr,
				SNotificationItem::CS_Success,
				IsBaseGameBuildTask() ?
					LOCTEXT("Notification_Title_BaseGame_Complete", "Aurora Project Packaging failed!") :
					FText::Format(LOCTEXT("Notification_Title_DLC_Complete", "Aurora DLC Packaging '{0}' complete!"), FText::FromString(DLCSettings.Config.DLCName))
			);
			Status = ELauncherTaskStatus::Completed;
		}
		else
		{	
			TGraphTask<FMainFrameActionsNotificationTask>::CreateTask().ConstructAndDispatchWhenReady(
				NotificationItemPtr,
				SNotificationItem::CS_Fail,
				IsBaseGameBuildTask() ?
					LOCTEXT("Notification_Title_BaseGame_Fail", "Aurora Project Packaging failed!") :
					FText::Format(LOCTEXT("Notification_Title_DLC_Fail", "Aurora DLC Packaging '{0}' failed!"), FText::FromString(DLCSettings.Config.DLCName)),
				FText(),
				false);
			Status = ELauncherTaskStatus::Failed;
		}
	}
}

void FAuroraBuildTask::LaunchCanceled(double ExecutionTime)
{
	UE_LOG(LogGFPakExporter, Log, TEXT("Profile launch command was canceled."));

	if (LauncherWorker)
	{
		TGraphTask<FMainFrameActionsNotificationTask>::CreateTask().ConstructAndDispatchWhenReady(
			NotificationItemPtr,
			SNotificationItem::CS_Fail,
			IsBaseGameBuildTask() ?
					LOCTEXT("Notification_Title_BaseGame_Cancel", "Aurora Project Packaging canceled!") :
					FText::Format(LOCTEXT("Notification_Title_DLC_Cancel", "Aurora DLC Packaging '{0}' canceled!"), FText::FromString(DLCSettings.Config.DLCName))
		);
		Status = ELauncherTaskStatus::Pending;
	}
}

bool FAuroraBuildTask::CreateNotification()
{
	const FText DLCName = FText::FromString(DLCSettings.Config.DLCName);
	{
		FScopeLock Lock(&NotificationTextMutex);
		NotificationText = FText::GetEmpty();
	}
	
	// As per FUATHelperModule::CreateUatTask
	FFormatNamedArguments Arguments;
	FText PlatformDisplayName = Profile->GetCookedPlatforms().IsEmpty() ? FText::GetEmpty() : FText::FromString(Profile->GetCookedPlatforms()[0]);
	Arguments.Add(TEXT("Platform"), FText::FromString(Profile->GetCookedPlatforms()[0]));
	Arguments.Add(TEXT("DLCName"), DLCName);
	FText NotificationFormat = (PlatformDisplayName.IsEmpty()) ? LOCTEXT("Notification_Title_NoPlatform", "Aurora Packaging...") : LOCTEXT("Notification_Title", "Aurora Packaging for {Platform}:\n{DLCName}");
	FNotificationInfo Info( FText::Format( NotificationFormat, Arguments) );
	
	Info.Image = FAppStyle::GetBrush(TEXT("MainFrame.CookContent"));
	Info.bFireAndForget = false;
	Info.FadeOutDuration = 0.0f;
	Info.ExpireDuration = 0.0f;
	Info.SubText = TAttribute<FText>::CreateSP(this, &FAuroraBuildTask::HandleNotificationGetText),
	Info.Hyperlink = FSimpleDelegate::CreateSP(this, &FAuroraBuildTask::HandleNotificationHyperlinkNavigateShowOutput);
	Info.HyperlinkText = LOCTEXT("ShowOutputLogHyperlink", "Show Output Log");
	Info.ButtonDetails.Add(
		FNotificationButtonInfo(
			LOCTEXT("UatTaskCancel", "Cancel"),
			LOCTEXT("UatTaskCancelToolTip", "Cancels execution of this task."),
			FSimpleDelegate::CreateSP(this, &FAuroraBuildTask::HandleNotificationCancelButtonClicked),
			SNotificationItem::CS_Pending
		)
	);
	Info.ButtonDetails.Add(
		FNotificationButtonInfo(
			LOCTEXT("UatTaskDismiss", "Dismiss"),
			FText(),
			FSimpleDelegate::CreateLambda([SharedThis = AsShared().ToSharedPtr()]() // We want to keep the task alive with the notification
			{
				if (SharedThis)
				{
					SharedThis->HandleNotificationDismissButtonClicked();
				}
			}),
			SNotificationItem::CS_Fail
		)
	);

	TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
	TSharedPtr<SNotificationItem> OldNotification = NotificationItemPtr.Pin();
	if(OldNotification.IsValid())
	{
		OldNotification->Fadeout();
	}
	if (ensure(NotificationItem.IsValid()))
	{
		NotificationItem->SetCompletionState(SNotificationItem::CS_Pending);
		NotificationItemPtr = NotificationItem;
		return true;
	}
	else
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("Unable to create a Notification"))
		NotificationItemPtr = nullptr;
		return false;
	}
}

FText FAuroraBuildTask::HandleNotificationGetText() const
{
	FScopeLock Lock(&NotificationTextMutex);
	return NotificationText;
}

void FAuroraBuildTask::HandleNotificationCancelButtonClicked()
{
	if (LauncherWorker.IsValid() )
	{
		LauncherWorker->Cancel();
	}
}

void FAuroraBuildTask::HandleNotificationDismissButtonClicked()
{
	TSharedPtr<SNotificationItem> NotificationItem = NotificationItemPtr.Pin();
	if (NotificationItem.IsValid())
	{
		NotificationItem->SetExpireDuration(0.0f);
		NotificationItem->SetFadeOutDuration(0.0f);
		NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
		NotificationItem->ExpireAndFadeout();
		NotificationItemPtr.Reset();
	}
}

void FAuroraBuildTask::HandleNotificationHyperlinkNavigateShowOutput()
{
	FOutputLogModule& OutputLogModule = FOutputLogModule::Get();
	OutputLogModule.FocusOutputLog();
}

#undef LOCTEXT_NAMESPACE
