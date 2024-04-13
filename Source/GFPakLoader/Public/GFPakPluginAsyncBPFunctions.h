// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"

#include "GFPakPlugin.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "GFPakPluginAsyncBPFunctions.generated.h"

class UGFPakPlugin;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPakPluginAsyncEvent);

/**
 * Class to call the Async UGFPakPlugin::ActivateGameFeature from Blueprints
 */
UCLASS()
class GFPAKLOADER_API UGFPakPluginActivateGameFeatureAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Activate the GameFeatures of this Pak Plugin. This function is Asynchronous and the CompleteDelegate callback will be called on completion.
	 * This has the same effect as activating the plugin with the GameFeaturesSubsystem, but keeps track of it and will ensure the plugin is unregistered when unmounted.
	 * The plugin will be Mounted if it was not.
	 * If successful, the Status of this instance will change first to  `ActivatingGameFeature` and then to `GameFeatureActivated`.
	 * If failed, the Status will end up reverting back to `Mounted`
	 * If the plugin was already Activated, the Status will not change.
	 * @param GFPakPlugin The Pak Plugin to activate
	 * @param OutGFPakPlugin Just returns the Pak Plugin that was passed in
	 */
	UFUNCTION(BlueprintCallable, DisplayName="Activate Game Feature", Category="GameFeatures Pak Loader", meta=(BlueprintInternalUseOnly="true"))
	static UGFPakPluginActivateGameFeatureAsync* GFPakPluginActivateGameFeatureAsync(UPARAM(DisplayName = "Pak Plugin") UGFPakPlugin* GFPakPlugin, UPARAM(DisplayName = "GF Pak Plugin") UGFPakPlugin*& OutGFPakPlugin);

	/** Called when the Pak Plugin successfully activated its GameFeatures. This might be called before the Then pin */
	UPROPERTY(BlueprintAssignable)
	FPakPluginAsyncEvent OnActivated;

	/** Called when the Pak Plugin failed activating its GameFeatures */
	UPROPERTY(BlueprintAssignable)
	FPakPluginAsyncEvent OnFailed;

	// Start UBlueprintAsyncActionBase Functions
	virtual void Activate() override;
	// End UBlueprintAsyncActionBase Functions
private:
	UPROPERTY()
	UGFPakPlugin* PakPlugin = nullptr;

	void ReportActivated();
	void ReportFailed();
};


/**
 * Class to call the Async UGFPakPlugin::DeactivateGameFeature from Blueprints
 */
UCLASS()
class GFPAKLOADER_API UGFPakPluginDeactivateGameFeatureAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Deactivate the GameFeatures of this Pak Plugin. This function is Asynchronous and the CompleteDelegate callback will be called on completion.
	 * This has the same effect as deactivating the plugin with the GameFeaturesSubsystem, but keeps track of it and will ensure the plugin is unregistered when unmounted.
	 * If successful, the Status of this instance will change first to  `DeactivatingGameFeature` and then to `Mounted`.
	 * If failed, the Status will end up reverting back to `Mounted`
	 * If the plugin was already Deactivated, the Status will not change.
	 * @param GFPakPlugin The Pak Plugin to deactivate
	 * @param OutGFPakPlugin Just returns the Pak Plugin that was passed in
	 */
	UFUNCTION(BlueprintCallable, DisplayName="Deactivate Game Feature", Category="GameFeatures Pak Loader", meta=(BlueprintInternalUseOnly="true"))
	static UGFPakPluginDeactivateGameFeatureAsync* GFPakPluginDeactivateGameFeatureAsync(UPARAM(DisplayName = "Pak Plugin") UGFPakPlugin* GFPakPlugin, UPARAM(DisplayName = "GF Pak Plugin") UGFPakPlugin*& OutGFPakPlugin);

	/** Called when the Pak Plugin successfully deactivated its GameFeatures. This might be called before the Then pin */
	UPROPERTY(BlueprintAssignable)
	FPakPluginAsyncEvent OnDeactivated;

	/** Called when the Pak Plugin failed activating its GameFeatures */
	UPROPERTY(BlueprintAssignable)
	FPakPluginAsyncEvent OnFailed;

	// Start UBlueprintAsyncActionBase Functions
	virtual void Activate() override;
	// End UBlueprintAsyncActionBase Functions
private:
	UPROPERTY()
	UGFPakPlugin* PakPlugin = nullptr;

	void ReportDeactivated();
	void ReportFailed();
};
