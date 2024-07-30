// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "GFPakExporterAssetManager.generated.h"

/**
 * Custom AssetManager class to redirect CookEvent via delegates
 */
UCLASS()
class GFPAKEXPORTERCOMMANDLET_API UGFPakExporterAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	/** Gets package names to add to a DLC cook*/
	virtual void ModifyDLCCook(const FString& DLCName, TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook) override;
	/**
	 * Allows for game code to modify the base packages that have been read in from the DevelopmentAssetRegistry when performing a DLC cook.
	 * Can be used to modify which packages should be considered to be already cooked.
	 * Any packages within the PackagesToClearResults will have their cook results cleared and be cooked again if requested by the cooker.
	 */
	virtual void ModifyDLCBasePackages(const ITargetPlatform* TargetPlatform, TArray<FName>& PlatformBasedPackages, TSet<FName>& PackagesToClearResults) const override;
	/**
	 * If the given package contains a primary asset, get the packages referenced by its AssetBundleEntries.
	 * Used to inform the cook of should-be-cooked dependencies of PrimaryAssets for PrimaryAssets that
	 * are recorded in the AssetManager but have cooktype Unknown and so are not returned from ModifyCook.
	 */
	virtual void ModifyCookReferences(FName PackageName, TArray<FName>& PackagesToCook) override;
	/** 
	  * Called immediately before saving the asset registry during cooking
	  */
	virtual void PreSaveAssetRegistry(const ITargetPlatform* TargetPlatform, const TSet<FName>& InCookedPackages) override;
	
	DECLARE_MULTICAST_DELEGATE_FourParams(FModifyDLCCookDelegate, const FString& /*DLCName*/, TConstArrayView<const ITargetPlatform*> /*TargetPlatforms*/, TArray<FName>& /*PackagesToCook*/, TArray<FName>& /*PackagesToNeverCook*/)
	FModifyDLCCookDelegate OnModifyDLCCookDelegate;
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FModifyDLCBasePackages, const ITargetPlatform* /*TargetPlatform*/, TArray<FName>& /*PlatformBasedPackages*/, TSet<FName>& /*PackagesToClearResults*/)
	FModifyDLCBasePackages OnModifyDLCBasePackagesDelegate;
};
