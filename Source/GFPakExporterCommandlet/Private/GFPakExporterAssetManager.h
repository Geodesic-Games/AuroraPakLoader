// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "GFPakExporterAssetManager.generated.h"

/**
 * 
 */
UCLASS()
class GFPAKEXPORTERCOMMANDLET_API UGFPakExporterAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	virtual void BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform) override;
	virtual void ClearAllCachedCookedPlatformData() override;
	virtual void ClearCachedCookedPlatformData(const ITargetPlatform* TargetPlatform) override;

private:
	virtual void CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform, TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile) override;

public:
	virtual void GetAdditionalAssetDataObjectsForCook(FArchiveCookContext& CookContext, TArray<UObject*>& OutObjects) const override;
	virtual bool IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform) override;
	virtual void WillNeverCacheCookedPlatformDataAgain() override;
	/** Gets package names to add to the cook, and packages to never cook even if in startup set memory or referenced */
	virtual void ModifyCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook) override;
	/** Gets package names to add to a DLC cook*/
	virtual void ModifyDLCCook(const FString& DLCName, TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook) override;
	/**
	 * Allows for game code to modify the base packages that have been read in from the DevelopmentAssetRegistry when performing a DLC cook.
	 * Can be used to modify which packages should be considered to be already cooked.
	 * Any packages within the PackagesToClearResults will have their cook results cleared and be cooked again if requested by the cooker.
	 */
	virtual void ModifyDLCBasePackages(const ITargetPlatform* TargetPlatform, TArray<FName>& PlatformBasedPackages, TSet<FName>& PackagesToClearResults) const override;
	virtual void ModifyCookReferences(FName PackageName, TArray<FName>& PackagesToCook) override;
	virtual bool ShouldCookForPlatform(const UPackage* Package, const ITargetPlatform* TargetPlatform) override;
	virtual EPrimaryAssetCookRule GetPackageCookRule(FName PackageName) const override;
	virtual bool VerifyCanCookPackage(UE::Cook::ICookInfo* CookInfo, FName PackageName, bool bLogError) const override;
	virtual void PreSaveAssetRegistry(const ITargetPlatform* TargetPlatform, const TSet<FName>& InCookedPackages) override;
	virtual bool HandleCookCommand(FStringView Token) override;

protected:
	virtual void GatherPublicAssetsForPackage(FName PackagePath, TArray<FName>& PackagesToCook) const override;
};
