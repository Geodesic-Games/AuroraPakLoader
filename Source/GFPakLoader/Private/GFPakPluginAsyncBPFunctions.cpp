// Copyright GeoTech BV


#include "GFPakPluginAsyncBPFunctions.h"

#include "GFPakPlugin.h"

UGFPakPluginActivateGameFeatureAsync* UGFPakPluginActivateGameFeatureAsync::GFPakPluginActivateGameFeatureAsync(UGFPakPlugin* GFPakPlugin, UGFPakPlugin*& OutGFPakPlugin)
{
	UGFPakPluginActivateGameFeatureAsync* AsyncAction = NewObject<UGFPakPluginActivateGameFeatureAsync>();
	AsyncAction->PakPlugin = GFPakPlugin;
	OutGFPakPlugin = GFPakPlugin;
	if (IsValid(GFPakPlugin) && IsValid(GFPakPlugin->GetWorld()))
	{
		AsyncAction->RegisterWithGameInstance(GFPakPlugin->GetWorld()->GetGameInstance());
	}
	return AsyncAction;
}

void UGFPakPluginActivateGameFeatureAsync::Activate()
{
	if(IsValid(PakPlugin))
	{
		auto Start = FPlatformTime::Cycles64();
		UE_LOG(LogTemp, Warning, TEXT("%lld ---===ActivateGameFeature===--- "), Start)
		PakPlugin->ActivateGameFeature(FOperationCompleted::CreateLambda(
			[Start, WeakThis = TWeakObjectPtr<UGFPakPluginActivateGameFeatureAsync>(this)](const bool bSuccessful, const TOptional<UE::GameFeatures::FResult>& Result)
		{
			UE_CLOG(!bSuccessful, LogTemp, Error, TEXT("%lld ---===FAILED ACTIVATING===---"), FPlatformTime::Cycles64() - Start);
			UE_CLOG(bSuccessful, LogTemp, Warning, TEXT("%lld ---===ACTIVATED===---"), FPlatformTime::Cycles64() - Start);
			UGFPakPluginActivateGameFeatureAsync* This = WeakThis.Get();
			if (IsValid(This))
			{
				if (This->RegisteredWithGameInstance.IsValid() && IsValid(This->PakPlugin) && bSuccessful)
				{
					This->ReportActivated();
				}
				else
				{
					This->ReportFailed();
				}
			}
		}));
		UE_LOG(LogTemp, Warning, TEXT("%lld ---===After calling ActivateGameFeature===---"), FPlatformTime::Cycles64() - Start)
	}
	else
	{
		// If something failed, we can broadcast OnFail, and then wrap up.
		ReportFailed();
	}
}

void UGFPakPluginActivateGameFeatureAsync::ReportActivated()
{
	OnActivated.Broadcast();
	SetReadyToDestroy();
}
void UGFPakPluginActivateGameFeatureAsync::ReportFailed()
{
	OnFailed.Broadcast();
	SetReadyToDestroy();
}



UGFPakPluginDeactivateGameFeatureAsync* UGFPakPluginDeactivateGameFeatureAsync::GFPakPluginDeactivateGameFeatureAsync(UGFPakPlugin* GFPakPlugin, UGFPakPlugin*& OutGFPakPlugin)
{
	UGFPakPluginDeactivateGameFeatureAsync* AsyncAction = NewObject<UGFPakPluginDeactivateGameFeatureAsync>();
	AsyncAction->PakPlugin = GFPakPlugin;
	OutGFPakPlugin = GFPakPlugin;
	if (IsValid(GFPakPlugin) && IsValid(GFPakPlugin->GetWorld()))
	{
		AsyncAction->RegisterWithGameInstance(GFPakPlugin->GetWorld()->GetGameInstance());
	}
	return AsyncAction;
}

void UGFPakPluginDeactivateGameFeatureAsync::Activate()
{
	if(IsValid(PakPlugin))
	{
		auto Start = FPlatformTime::Cycles64();
		UE_LOG(LogTemp, Warning, TEXT("%lld ---===DeactivateGameFeature===--- "), Start)
		PakPlugin->DeactivateGameFeature(FOperationCompleted::CreateLambda(
			[Start, WeakThis = TWeakObjectPtr<UGFPakPluginDeactivateGameFeatureAsync>(this)](const bool bSuccessful, const TOptional<UE::GameFeatures::FResult>& Result)
		{
			UE_CLOG(!bSuccessful, LogTemp, Error, TEXT("%lld ---===FAILED DEACTIVATING===---"), FPlatformTime::Cycles64() - Start);
			UE_CLOG(bSuccessful, LogTemp, Warning, TEXT("%lld ---===DEACTIVATED===---"), FPlatformTime::Cycles64() - Start);
			UGFPakPluginDeactivateGameFeatureAsync* This = WeakThis.Get();
			if (IsValid(This))
			{
				if (This->RegisteredWithGameInstance.IsValid() && IsValid(This->PakPlugin) && bSuccessful)
				{
					This->ReportDeactivated();
				}
				else
				{
					This->ReportFailed();
				}
			}
		}));
		UE_LOG(LogTemp, Warning, TEXT("%lld ---===After calling DeactivateGameFeature===---"), FPlatformTime::Cycles64() - Start)
	}
	else
	{
		// If something failed, we can broadcast OnFail, and then wrap up.
		ReportFailed();
	}
}

void UGFPakPluginDeactivateGameFeatureAsync::ReportDeactivated()
{
	OnDeactivated.Broadcast();
	SetReadyToDestroy();
}
void UGFPakPluginDeactivateGameFeatureAsync::ReportFailed()
{
	OnFailed.Broadcast();
	SetReadyToDestroy();
}
