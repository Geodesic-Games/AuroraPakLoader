// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"
#include "AuroraExporterSettings.h"
#include "ILauncher.h"
#include "ILauncherWorker.h"



struct FAuroraBuildTask : public TSharedFromThis<FAuroraBuildTask>
{
public:
	FAuroraBuildTask(const ILauncherProfilePtr& InProfile, const FAuroraExporterSettings& InSettings)
		: Profile(InProfile), Settings(InSettings) {}

	bool Launch(const ILauncherPtr& Launcher);
	void Cancel();
	
	ELauncherTaskStatus::Type GetStatus() const { return Status; };
	ILauncherProfilePtr GetProfile() const { return Profile; }
	ILauncherWorkerPtr GetLauncherWorker() const { return LauncherWorker; }
private:
	ILauncherProfilePtr Profile{};
	FAuroraExporterSettings Settings;
	ELauncherTaskStatus::Type Status = ELauncherTaskStatus::Pending;
	ILauncherWorkerPtr LauncherWorker{};
	
	void MessageReceived(const FString& InMessage);
	void HandleStageStarted(const FString& InStage);
	void HandleStageCompleted(const FString& InStage, double StageTime);
	void LaunchCompleted(bool Outcome, double ExecutionTime, int32 ReturnCode);
	void LaunchCanceled(double ExecutionTime);

private: // Notification
	bool CreateNotification();
	
	TWeakPtr<SNotificationItem> NotificationItemPtr;
	mutable FCriticalSection NotificationTextMutex;
	FText NotificationText;
	
	FText HandleNotificationGetText() const;
	void HandleNotificationCancelButtonClicked();
	void HandleNotificationDismissButtonClicked();
	void HandleNotificationHyperlinkNavigateShowOutput();
};
