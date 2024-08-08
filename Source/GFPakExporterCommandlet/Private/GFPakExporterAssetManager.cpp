// Copyright GeoTech BV


#include "GFPakExporterAssetManager.h"

#include "GFPakExporterCommandletLog.h"


void UGFPakExporterAssetManager::ModifyCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook)
{
	Super::ModifyCook(TargetPlatforms, PackagesToCook, PackagesToNeverCook);
	
	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("ModifyCook:  TargetPlatforms: %d  PackagesToCook: %d   PackagesToNeverCook: %d"),
		TargetPlatforms.Num(), PackagesToCook.Num(), PackagesToNeverCook.Num())
	
	OnModifyCookDelegate.Broadcast(TargetPlatforms, PackagesToCook, PackagesToNeverCook);
	
	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("ModifyCook POST delegate:  TargetPlatforms: %d  PackagesToCook: %d   PackagesToNeverCook: %d"),
		TargetPlatforms.Num(), PackagesToCook.Num(), PackagesToNeverCook.Num())
}

EPrimaryAssetCookRule UGFPakExporterAssetManager::GetPackageCookRule(FName PackageName) const
{
	const EPrimaryAssetCookRule Rule = Super::GetPackageCookRule(PackageName);
	
	if (OnGetPackageCookRule.IsBound())
	{
		const EPrimaryAssetCookRule AdjustedRule = OnGetPackageCookRule.Execute(Rule, PackageName);
		if (AdjustedRule != Rule)
		{
			UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("GetPackageCookRule:  Package: '%s' rule '%s' changed to '%s'"),
				*PackageName.ToString(), *UEnum::GetValueAsString(Rule), *UEnum::GetValueAsString(AdjustedRule))
			
			return AdjustedRule;
		}
	}
	return Rule;
}

void UGFPakExporterAssetManager::ModifyDLCCook(const FString& DLCName, TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook)
{
	Super::ModifyDLCCook(DLCName, TargetPlatforms, PackagesToCook, PackagesToNeverCook);
	
	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("ModifyDLCCook:  DLCName: '%s'  TargetPlatforms: %d  PackagesToCook: %d   PackagesToNeverCook: %d"),
		*DLCName, TargetPlatforms.Num(), PackagesToCook.Num(), PackagesToNeverCook.Num())
	
	OnModifyDLCCookDelegate.Broadcast(DLCName, TargetPlatforms, PackagesToCook, PackagesToNeverCook);

	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("ModifyDLCCook POST Delegate:  DLCName: '%s'  TargetPlatforms: %d  PackagesToCook: %d   PackagesToNeverCook: %d"),
		*DLCName, TargetPlatforms.Num(), PackagesToCook.Num(), PackagesToNeverCook.Num())
}

void UGFPakExporterAssetManager::ModifyDLCBasePackages(const ITargetPlatform* TargetPlatform, TArray<FName>& PlatformBasedPackages, TSet<FName>& PackagesToClearResults) const
{
	Super::ModifyDLCBasePackages(TargetPlatform, PlatformBasedPackages, PackagesToClearResults);
	
	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("ModifyDLCBasePackages:  TargetPlatform: '%s'  PlatformBasedPackages: %d   PackagesToClearResults: %d"),
		*TargetPlatform->DisplayName().ToString(), PlatformBasedPackages.Num(), PackagesToClearResults.Num())

	
	OnModifyDLCBasePackagesDelegate.Broadcast(TargetPlatform, PlatformBasedPackages, PackagesToClearResults);

	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("ModifyDLCBasePackages POST Delegate:  TargetPlatform: '%s'  PlatformBasedPackages: %d   PackagesToClearResults: %d"),
		*TargetPlatform->DisplayName().ToString(), PlatformBasedPackages.Num(), PackagesToClearResults.Num())
}

void UGFPakExporterAssetManager::PreSaveAssetRegistry(const ITargetPlatform* TargetPlatform, const TSet<FName>& InCookedPackages)
{
	Super::PreSaveAssetRegistry(TargetPlatform, InCookedPackages);
	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("PreSaveAssetRegistry:  TargetPlatform: '%s'   InCookedPackages; %d"), *TargetPlatform->PlatformName(), InCookedPackages.Num())
	
	UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT("Assets saved in the Pak AssetRegistry: %d"), InCookedPackages.Num())
	for (const FName& PackageName : InCookedPackages)
	{
		UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT(" - '%s'"), *PackageName.ToString())
	}
}
