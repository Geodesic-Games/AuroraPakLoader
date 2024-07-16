// Copyright GeoTech BV


#include "GFPakExporterAssetManager.h"

#include "GFPakExporterCommandletLog.h"


void UGFPakExporterAssetManager::ModifyDLCCook(const FString& DLCName, TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook)
{
	Super::ModifyDLCCook(DLCName, TargetPlatforms, PackagesToCook, PackagesToNeverCook);
	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("ModifyDLCCook:  DLCName: '%s'  TargetPlatforms: %d  PackagesToCook: %d   PackagesToNeverCook: %d"),
		*DLCName, TargetPlatforms.Num(), PackagesToCook.Num(), PackagesToNeverCook.Num())
	
	
	OnModifyDLCCookDelegate.Broadcast(DLCName, TargetPlatforms, PackagesToCook, PackagesToNeverCook);

	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("ModifyDLCCook Post Delegate:  DLCName: '%s'  TargetPlatforms: %d  PackagesToCook: %d   PackagesToNeverCook: %d"),
		*DLCName, TargetPlatforms.Num(), PackagesToCook.Num(), PackagesToNeverCook.Num())
	
	UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT(" == PackagesToCook: %d"), PackagesToCook.Num())
	for (const FName& Package : PackagesToCook)
	{
		UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT(" - [Cook]:  '%s'"), *Package.ToString())
	}

	UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT(" == PackagesTo NEVER Cook: %d"), PackagesToNeverCook.Num())
	for (const FName& Package : PackagesToNeverCook)
	{
		UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT(" - [NEVER Cook]:  '%s'"), *Package.ToString())
	}
}

void UGFPakExporterAssetManager::ModifyDLCBasePackages(const ITargetPlatform* TargetPlatform, TArray<FName>& PlatformBasedPackages, TSet<FName>& PackagesToClearResults) const
{
	Super::ModifyDLCBasePackages(TargetPlatform, PlatformBasedPackages, PackagesToClearResults);
	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("ModifyDLCBasePackages:  TargetPlatform: '%s'  PlatformBasedPackages: %d   PackagesToClearResults: %d"),
		*TargetPlatform->DisplayName().ToString(), PlatformBasedPackages.Num(), PackagesToClearResults.Num())

	
	OnModifyDLCBasePackagesDelegate.Broadcast(TargetPlatform, PlatformBasedPackages, PackagesToClearResults);

	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("ModifyDLCBasePackages Post Delegate:  TargetPlatform: '%s'  PlatformBasedPackages: %d   PackagesToClearResults: %d"),
		*TargetPlatform->DisplayName().ToString(), PlatformBasedPackages.Num(), PackagesToClearResults.Num())
	
	UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT(" == PlatformBasedPackages: %d"), PlatformBasedPackages.Num())
	for (const FName& Package : PlatformBasedPackages)
	{
		UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT(" - [Base]:  '%s'"), *Package.ToString())
	}
	UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT(" == PackagesToClearResults: %d"), PackagesToClearResults.Num())
	for (const FName& Package : PackagesToClearResults)
	{
		UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT(" - [Clear]:  '%s'"), *Package.ToString())
	}
}

void UGFPakExporterAssetManager::ModifyCookReferences(FName PackageName, TArray<FName>& PackagesToCook)
{
	Super::ModifyCookReferences(PackageName, PackagesToCook);
	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("ModifyCookReferences:  PackageName: '%s'   PackagesToCook: %d"), *PackageName.ToString(), PackagesToCook.Num())
}

void UGFPakExporterAssetManager::PreSaveAssetRegistry(const ITargetPlatform* TargetPlatform, const TSet<FName>& InCookedPackages)
{
	Super::PreSaveAssetRegistry(TargetPlatform, InCookedPackages);
	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("PreSaveAssetRegistry:  TargetPlatform: '%s'   InCookedPackages; %d"), *TargetPlatform->PlatformName(), InCookedPackages.Num())
	
	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("Assets saved in the Pak AssetRegistry: %d"), InCookedPackages.Num())
	for (const FName& Package : InCookedPackages)
	{
		UE_LOG(LogGFPakExporterCommandlet, Display, TEXT(" - '%s'"), *Package.ToString())
	}
}
