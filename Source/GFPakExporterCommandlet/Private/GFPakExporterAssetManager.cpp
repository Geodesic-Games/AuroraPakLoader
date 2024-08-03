// Copyright GeoTech BV


#include "GFPakExporterAssetManager.h"

#include "GFPakExporterCommandletLog.h"
#include "AssetRegistry/AssetRegistryState.h"


bool UGFPakExporterAssetManager::HandleCookCommand(FStringView Token)
{
	UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("HandleCookCommand:  Token: '%s'"), Token.GetData())
	return Super::HandleCookCommand(Token);
}

void UGFPakExporterAssetManager::CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform, TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile)
{
	UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("CookAdditionalFilesOverride:  PackageFilename: '%s'"), PackageFilename)
}

void UGFPakExporterAssetManager::GetAdditionalAssetDataObjectsForCook(FArchiveCookContext& CookContext, TArray<UObject*>& OutObjects) const
{
	Super::GetAdditionalAssetDataObjectsForCook(CookContext, OutObjects);
	UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("GetAdditionalAssetDataObjectsForCook:  CookContext: '%s'"), TEXT(""))
}

void UGFPakExporterAssetManager::ModifyCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook)
{
	// this->AddDynamicAsset()
	// IAssetRegistry& AssetRegistry = GetAssetRegistry();
	// CustomAssetLabel = NewObject<UAuroraAssetLabel>(this, FName{TEXT("UGFPakExporterAssetManager_AssetLabel")});
	// CustomAssetLabel->Rules.Priority = std::numeric_limits<int32>::max();
	// CustomAssetLabel->Rules.CookRule = EPrimaryAssetCookRule::Unknown;
	// AssetRegistry.AssetCreated(CustomAssetLabel);
	//
	// FPrimaryAssetId PrimaryAssetId = CustomAssetLabel->GetPrimaryAssetId();
	// FAssetBundleData Bundle;
	// AddDynamicAsset(PrimaryAssetId, FSoftObjectPath(),Bundle);
	// SetPrimaryAssetRules(PrimaryAssetId, CustomAssetLabel->Rules);
	
	Super::ModifyCook(TargetPlatforms, PackagesToCook, PackagesToNeverCook);
	// for (auto& PackageName : PackagesToCook)
	// {
	// 	if (PackageName.ToString().StartsWith(TEXT("/Game/InfinityBladeGrassLands/")))
	// 	{
	// 		UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("GetPackageCookRule:  Package: '%s'"), *PackageName.ToString())
	// 		//todo: they might be referenced, when to check?
	// 		// PackagesRemovedFromCook.Add(PackageName);
	// 		// PackagesToNeverCook.Add(PackageName);
	// 	}
	// }
	// for (auto& PackageName : PackagesRemovedFromCook)
	// {
	// 	PackagesToCook.Remove(PackageName);
	// }
	OnModifyCookDelegate.Broadcast(TargetPlatforms, PackagesToCook, PackagesToNeverCook);
	
	UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("ModifyCook:  TargetPlatforms: %d  PackagesToCook: %d   PackagesToNeverCook: %d"),
		TargetPlatforms.Num(), PackagesToCook.Num(), PackagesToNeverCook.Num())
}

bool UGFPakExporterAssetManager::ShouldCookForPlatform(const UPackage* Package, const ITargetPlatform* TargetPlatform)
{
	// UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("ShouldCookForPlatform:  Package: '%s'"), *GetFullNameSafe(Package))
	return Super::ShouldCookForPlatform(Package, TargetPlatform);
}

EPrimaryAssetCookRule UGFPakExporterAssetManager::GetPackageCookRule(FName PackageName) const
{
	// if (PackageName.ToString().StartsWith(TEXT("/Game/InfinityBladeGrassLands/")))
	// {
	// 	CustomAssetLabel->ExplicitAssets.Add(TSoftObjectPtr(FSoftObjectPath(PackageName, FString{})));
	//
	// 	// IAssetRegistry& AssetRegistry = GetAssetRegistry();
	// 	// TArray<FAssetData> AssetDatas;
	// 	// AssetRegistry.GetAssetsByPackageName(PackageName, AssetDatas, false, false);
	// 	// FAssetPackageData PackageData;
	// 	// AssetRegistry.TryGetAssetPackageData(PackageName, PackageData);
	// 	//
	// 	// FAssetRegistryState OldState{}; // Create a temporary AssetRegistryState as this is the only exposed way to update a state without the UObject
	// 	// for (FAssetData& AssetData : AssetDatas)
	// 	// {
	// 	// 	FAssetData* OldGameData = new FAssetData(AssetData);
	// 	// 	OldState.AddAssetData(OldGameData); // needs to be a pointer to a new object! will be destroyed in the FAssetRegistryState destructor 
	// 	// }
	// 	// AssetRegistry.AppendState(OldState);
	// 	//
	// 	// FPackagePath PackagePath;
	// 	// if (FPackagePath::TryFromPackageName(PackageName, PackagePath))
	// 	// {
	// 	// 	FPackagePath OutPackagePath;
	// 	// 	const FPackageName::EPackageLocationFilter PackageLocation = FPackageName::DoesPackageExistEx(PackagePath, FPackageName::EPackageLocationFilter::Any, /*bMatchCaseOnDisk*/ false, &OutPackagePath);
	// 	// 	if (PackageLocation != FPackageName::EPackageLocationFilter::None)
	// 	// 	{
	// 	// 		AssetRegistry.ScanModifiedAssetFiles({OutPackagePath.GetLocalFullPath()});
	// 	// 	}
	// 	// }
	// 	
	// 	// TMap<FPrimaryAssetId, UE::AssetRegistry::EDependencyProperty> Managers;
	// 	// GetPackageManagers(PackageName, true, Managers); //todo how to get the right package manager? could also override!! => seems easier
	// 	//
	// 	// for ( auto Manager : Managers)
	// 	// {
	// 	// 	const FPrimaryAssetRulesExplicitOverride* FoundRulesOverride = AssetRuleOverrides.Find(Manager.Key);
	// 	// 	// if (AssetRuleOverrides.Find(PrimaryAssetId))
	// 	// }
	// 	
	// }
	EPrimaryAssetCookRule Rule = Super::GetPackageCookRule(PackageName);
	// if (PackageName.ToString().StartsWith(TEXT("/Game/InfinityBladeGrassLands/")))
	// {
	// 	EPrimaryAssetCookRule PreviousRule = Rule;
	// 	Rule = EPrimaryAssetCookRule::Unknown;
	// 	UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("GetPackageCookRule:  Package: '%s' => '%s' changed to '%s'"),
	// 		*PackageName.ToString(), *UEnum::GetValueAsString(PreviousRule), *UEnum::GetValueAsString(Rule))
	// 	//todo: they might be referenced, when to check?
	// }
	if (OnGetPackageCookRule.IsBound())
	{
		EPrimaryAssetCookRule AdjustedRule = OnGetPackageCookRule.Execute(Rule, PackageName);
		if (AdjustedRule != Rule)
		{
			UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("GetPackageCookRule:  Package: '%s' => '%s' changed to '%s'"),
				*PackageName.ToString(), *UEnum::GetValueAsString(Rule), *UEnum::GetValueAsString(AdjustedRule))
			//todo: handle hard references that are not included right now
			return AdjustedRule;
		}
	}
	return Rule;
}

bool UGFPakExporterAssetManager::VerifyCanCookPackage(UE::Cook::ICookInfo* CookInfo, FName PackageName, bool bLogError) const
{
	// UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("VerifyCanCookPackage:  Package: '%s'"), *PackageName.ToString())
	return Super::VerifyCanCookPackage(CookInfo, PackageName, bLogError);
}

bool UGFPakExporterAssetManager::GetPackageManagers(FName PackageName, bool bRecurseToParents, TMap<FPrimaryAssetId, UE::AssetRegistry::EDependencyProperty>& Managers) const
{
	bool bFoundAny = Super::GetPackageManagers(PackageName, bRecurseToParents, Managers);

	// if (PackageName.ToString().StartsWith(TEXT("/Game/InfinityBladeGrassLands/")) && CustomAssetLabel)
	// {
	// 	CustomAssetLabel->ExplicitAssets.AddUnique(TSoftObjectPtr(FSoftObjectPath(PackageName, FString{})));
	// 	
	// 	for ( auto It = Managers.CreateIterator(); It; ++It)
	// 	{
	// 		FPrimaryAssetRules Rules = GetPrimaryAssetRules(It->Key);
	// 		if (Rules.CookRule == EPrimaryAssetCookRule::AlwaysCook || Rules.CookRule == EPrimaryAssetCookRule::DevelopmentAlwaysCook)
	// 		{
	// 			It.RemoveCurrent();
	// 		}
	// 	}
	// 	
	// 	// Call FindOrAdd with -1 so we can use value != -1 to decide whether it already existed
	// 	UE::AssetRegistry::EDependencyProperty& DependencyProperty = Managers.FindOrAdd(CustomAssetLabel->GetPrimaryAssetId(), (UE::AssetRegistry::EDependencyProperty)-1);
	// 	// if (DependencyProperty == (UE::AssetRegistry::EDependencyProperty)-1)
	// 	// {
	// 		DependencyProperty = UE::AssetRegistry::EDependencyProperty::ManageMask;
	// 		return true;
	// 		// // Add to end of list to recurse into the parent.
	// 		// FAssetDependency& Added = ReferencingPrimaryAssets.Emplace_GetRef();
	// 		// Added.AssetId = Manager;
	// 		// Added.Category = IndirectCategory;
	// 		// // Set the parent's property equal to the child's properties, but change it to Indirect
	// 		// Added.Properties = IndirectProperties;
	// 	// }
	// }
	
	return bFoundAny;
}

void UGFPakExporterAssetManager::GatherPublicAssetsForPackage(FName PackagePath, TArray<FName>& PackagesToCook) const
{
	Super::GatherPublicAssetsForPackage(PackagePath, PackagesToCook);
	UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("GatherPublicAssetsForPackage:  Package: '%s'"), *PackagePath.ToString())
}

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
	for (auto Package : PackagesToCook)
	{
		if (Package.ToString().StartsWith(TEXT("/Game/InfinityBladeGrassLands/")))
		{
			UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("ModifyCookReferences:  PackageName: '%s'   PackagesToCook: '%s' [%d]"),
				*PackageName.ToString(), *Package.ToString(), PackagesToCook.Num())
		}
	}
	UE_LOG(LogGFPakExporterCommandlet, VeryVerbose, TEXT("ModifyCookReferences:  PackageName: '%s'   PackagesToCook: %d"), *PackageName.ToString(), PackagesToCook.Num())
}

void UGFPakExporterAssetManager::PreSaveAssetRegistry(const ITargetPlatform* TargetPlatform, const TSet<FName>& InCookedPackages)
{
	Super::PreSaveAssetRegistry(TargetPlatform, InCookedPackages);
	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("PreSaveAssetRegistry:  TargetPlatform: '%s'   InCookedPackages; %d"), *TargetPlatform->PlatformName(), InCookedPackages.Num())
	
	UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("Assets saved in the Pak AssetRegistry: %d"), InCookedPackages.Num())
	for (const FName& PackageName : InCookedPackages)
	{
		UE_LOG(LogGFPakExporterCommandlet, Display, TEXT(" - '%s'"), *PackageName.ToString())
	}
}
