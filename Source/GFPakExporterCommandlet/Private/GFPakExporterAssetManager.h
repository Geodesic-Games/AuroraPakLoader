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
	/** Gets package names to add to the cook, and packages to never cook even if in startup set memory or referenced */
	virtual void ModifyCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook) override;
	
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FModifyCookDelegate, TConstArrayView<const ITargetPlatform*> /*TargetPlatforms*/, TArray<FName>& /*PackagesToCook*/, TArray<FName>& /*PackagesToNeverCook*/)
	FModifyCookDelegate OnModifyCookDelegate;
	
	/** Returns cook rule for a package name using Management rules, games should override this to take into account their individual workflows */
	virtual EPrimaryAssetCookRule GetPackageCookRule(FName PackageName) const override;
	
	DECLARE_DELEGATE_RetVal_TwoParams(EPrimaryAssetCookRule, FGetPackageCookRule, EPrimaryAssetCookRule /*CurrentCookRule*/, FName /*PackageName*/)
	FGetPackageCookRule OnGetPackageCookRule;
	
	/** Gets package names to add to a DLC cook*/
	virtual void ModifyDLCCook(const FString& DLCName, TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook) override;

	DECLARE_MULTICAST_DELEGATE_FourParams(FModifyDLCCookDelegate, const FString& /*DLCName*/, TConstArrayView<const ITargetPlatform*> /*TargetPlatforms*/, TArray<FName>& /*PackagesToCook*/, TArray<FName>& /*PackagesToNeverCook*/)
	FModifyDLCCookDelegate OnModifyDLCCookDelegate;
	
	/**
	 * Allows for game code to modify the base packages that have been read in from the DevelopmentAssetRegistry when performing a DLC cook.
	 * Can be used to modify which packages should be considered to be already cooked.
	 * Any packages within the PackagesToClearResults will have their cook results cleared and be cooked again if requested by the cooker.
	 */
	virtual void ModifyDLCBasePackages(const ITargetPlatform* TargetPlatform, TArray<FName>& PlatformBasedPackages, TSet<FName>& PackagesToClearResults) const override;

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FModifyDLCBasePackages, const ITargetPlatform* /*TargetPlatform*/, TArray<FName>& /*PlatformBasedPackages*/, TSet<FName>& /*PackagesToClearResults*/)
	FModifyDLCBasePackages OnModifyDLCBasePackagesDelegate;
	
	/** 
	  * Called immediately before saving the asset registry during cooking
	  */
	virtual void PreSaveAssetRegistry(const ITargetPlatform* TargetPlatform, const TSet<FName>& InCookedPackages) override;
};
