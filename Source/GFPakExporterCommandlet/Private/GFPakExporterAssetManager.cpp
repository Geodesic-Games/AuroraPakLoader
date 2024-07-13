// Fill out your copyright notice in the Description page of Project Settings.


#include "GFPakExporterAssetManager.h"
#include "GFPakExporterCommandletLog.h"
#include "UObject/ArchiveCookContext.h"

void UGFPakExporterAssetManager::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	// UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("BeginCacheForCookedPlatformData:  Platform: '%s'"), *TargetPlatform->PlatformName())
	Super::BeginCacheForCookedPlatformData(TargetPlatform);
}

void UGFPakExporterAssetManager::ClearAllCachedCookedPlatformData()
{
	// UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("ClearAllCachedCookedPlatformData"))
	Super::ClearAllCachedCookedPlatformData();
}

void UGFPakExporterAssetManager::ClearCachedCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	// UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("BeginCacheForCookedPlatformData:  Platform: '%s'"), *TargetPlatform->PlatformName())
	Super::ClearCachedCookedPlatformData(TargetPlatform);
}

void UGFPakExporterAssetManager::CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform, TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile)
{
	// UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("CookAdditionalFilesOverride:   Platform: '%s'  PackageFilename: '%s'"), *TargetPlatform->PlatformName(), PackageFilename)
}

void UGFPakExporterAssetManager::GetAdditionalAssetDataObjectsForCook(FArchiveCookContext& CookContext, TArray<UObject*>& OutObjects) const
{
	// UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("GetAdditionalAssetDataObjectsForCook:   CookContext: ''  OutObjects: %d"), OutObjects.Num())
	Super::GetAdditionalAssetDataObjectsForCook(CookContext, OutObjects);
}

bool UGFPakExporterAssetManager::IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform)
{
	// UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("IsCachedCookedPlatformDataLoaded:  Platform: '%s'"), *TargetPlatform->PlatformName())
	return Super::IsCachedCookedPlatformDataLoaded(TargetPlatform);
}

void UGFPakExporterAssetManager::WillNeverCacheCookedPlatformDataAgain()
{
	// UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("WillNeverCacheCookedPlatformDataAgain"))
	Super::WillNeverCacheCookedPlatformDataAgain();
}

void UGFPakExporterAssetManager::ModifyCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook)
{
	Super::ModifyCook(TargetPlatforms, PackagesToCook, PackagesToNeverCook);
	UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("ModifyCook:  PackagesToCook: %d   PackagesToNeverCook: %d"), PackagesToCook.Num(), PackagesToNeverCook.Num())
	
}

void UGFPakExporterAssetManager::ModifyDLCCook(const FString& DLCName, TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook)
{
	Super::ModifyDLCCook(DLCName, TargetPlatforms, PackagesToCook, PackagesToNeverCook);
	UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("ModifyDLCCook:  DLCName: '%s'  PackagesToCook: %d   PackagesToNeverCook: %d"), *DLCName, PackagesToCook.Num(), PackagesToNeverCook.Num())

	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT(" == PackagesToCook: %d"), PackagesToCook.Num())
	for (auto& Package: PackagesToCook)
	{
		UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT(" - [Cook]:  '%s'"), *Package.ToString())
	}

	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT(" == PackagesTo NEVER Cook: %d"), PackagesToNeverCook.Num())
	for (auto& Package: PackagesToNeverCook)
	{
		UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT(" - [NEVER Cook]:  '%s'"), *Package.ToString())
	}
}

void UGFPakExporterAssetManager::ModifyDLCBasePackages(const ITargetPlatform* TargetPlatform, TArray<FName>& PlatformBasedPackages, TSet<FName>& PackagesToClearResults) const
{
	Super::ModifyDLCBasePackages(TargetPlatform, PlatformBasedPackages, PackagesToClearResults);
	UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("ModifyDLCBasePackages:  PlatformBasedPackages: %d   PackagesToClearResults: %d"), PlatformBasedPackages.Num(), PackagesToClearResults.Num())

	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT(" == PlatformBasedPackages: %d"), PlatformBasedPackages.Num())
	for (auto& Package: PlatformBasedPackages)
	{
		UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT(" - [Base]:  '%s'"), *Package.ToString())
	}
	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT(" == PackagesToClearResults: %d"), PackagesToClearResults.Num())
	for (auto& Package: PackagesToClearResults)
	{
		UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT(" - [Clear]:  '%s'"), *Package.ToString())
	}

	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT(""))
	UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT(" == Add Assets:"))
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry"); //todo: still ends up cooking and copying the base assets, need to check other function
	FName DLCFName{TEXT("Style02")};
	AssetRegistryModule.Get().EnumerateAllAssets([&PlatformBasedPackages, &PackagesToClearResults, DLCFName](const FAssetData& AssetData)
	{
		FName MountPoint = FPackageName::GetPackageMountPoint(AssetData.PackagePath.ToString());
		FString Filename;
		if (FPackageName::DoesPackageExist(AssetData.PackageName.ToString(), &Filename))
		{
			if (MountPoint != DLCFName)
			{
				// PlatformBasedPackages.Add(FName{Filename});
				UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT(" - [Add Base]:  '%s'"), *AssetData.PackageName.ToString())
			}
			else
			{
				PackagesToClearResults.Add(FName{Filename});
				UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT(" - [Add Clear]:  '%s'"), *AssetData.PackageName.ToString())
			}
		}
		return true;
	}, UE::AssetRegistry::EEnumerateAssetsFlags::OnlyOnDiskAssets);
}

void UGFPakExporterAssetManager::ModifyCookReferences(FName PackageName, TArray<FName>& PackagesToCook)
{
	// UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("ModifyCookReferences:  PackageName: '%s'   PackagesToCook: %d"), *PackageName.ToString(), PackagesToCook.Num())
	Super::ModifyCookReferences(PackageName, PackagesToCook);
}

bool UGFPakExporterAssetManager::ShouldCookForPlatform(const UPackage* Package, const ITargetPlatform* TargetPlatform)
{
	// UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("ShouldCookForPlatform:  Package: '%s'   TargetPlatform: '%s'"), *GetNameSafe(Package), *TargetPlatform->PlatformName())
	return Super::ShouldCookForPlatform(Package, TargetPlatform);
}

EPrimaryAssetCookRule UGFPakExporterAssetManager::GetPackageCookRule(FName PackageName) const
{
	// UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("GetPackageCookRule:  Package: '%s'"), *PackageName.ToString())
	// // return Super::GetPackageCookRule(PackageName);
	// FString PackageNameStr{PackageName.ToString()};
	// FName MountPoint = FPackageName::GetPackageMountPoint(PackageNameStr);
	//
	// return MountPoint == FName(TEXT("Style02")) ? EPrimaryAssetCookRule::AlwaysCook : EPrimaryAssetCookRule::NeverCook;
	return Super::GetPackageCookRule(PackageName);
}

bool UGFPakExporterAssetManager::VerifyCanCookPackage(UE::Cook::ICookInfo* CookInfo, FName PackageName, bool bLogError) const
{
	// UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("VerifyCanCookPackage:  Package: '%s'"), *PackageName.ToString())
	return Super::VerifyCanCookPackage(CookInfo, PackageName, bLogError);
}

void UGFPakExporterAssetManager::PreSaveAssetRegistry(const ITargetPlatform* TargetPlatform, const TSet<FName>& InCookedPackages)
{
	// UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("PreSaveAssetRegistry:  Package: '%s'   InCookedPackages; %d"), *TargetPlatform->PlatformName(), InCookedPackages.Num())
	Super::PreSaveAssetRegistry(TargetPlatform, InCookedPackages);
}

bool UGFPakExporterAssetManager::HandleCookCommand(FStringView Token)
{
	UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("HandleCookCommand:  Token: '%s'"), *FString(Token))
	return Super::HandleCookCommand(Token);
}

void UGFPakExporterAssetManager::GatherPublicAssetsForPackage(FName PackagePath, TArray<FName>& PackagesToCook) const
{
	// UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("GatherPublicAssetsForPackage:  PackagePath: '%s'   PackagesToCook: %d"), *PackagePath.ToString(), PackagesToCook.Num())
	Super::GatherPublicAssetsForPackage(PackagePath, PackagesToCook);
}
