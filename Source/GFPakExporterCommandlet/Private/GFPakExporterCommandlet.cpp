// Copyright GeoTech BV


#include "GFPakExporterCommandlet.h"

#include "GFPakExporter.h"
#include "GFPakExporterAssetManager.h"
#include "GFPakExporterCommandletLog.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "Elements/Framework/EngineElements.h"
#include "HAL/FileManagerGeneric.h"
#include "Settings/ProjectPackagingSettings.h"


#define LOCTEXT_NAMESPACE "FGFPakExporterCommandletModule"


const FString FGFPakExporterCommandletModule::PluginName{TEXT("AuroraPakLoader")};
const FName FGFPakExporterCommandletModule::ModuleName{TEXT("GFPakExporterCommandlet")};


void FGFPakExporterCommandletModule::StartupModule()
{
    if (!IsRunningCommandlet())
    {
        UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("FGFPakExporterCommandletModule::StartupModule:  Not running a commandlet"))
        return;
    }
    
    UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("FGFPakExporterCommandletModule::StartupModule: Running a commandlet"))

    FDelayedAutoRegisterHelper(EDelayedRegisterRunPhase::ObjectSystemReady, []()
    {
        if (FGFPakExporterCommandletModule::GetPtr() && FGFPakExporterCommandletModule::GetPtr()->CheckCommandLineAndAdjustSettings())
        {
            // We need to create the AssetManager at the right time, after GEngine is created but before UE creates the default one
            OnRegisterEngineElementsDelegate.AddLambda([]()
            {
                UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("OnRegisterEngineElementsDelegate"))
                if (FGFPakExporterCommandletModule* This = FGFPakExporterCommandletModule::GetPtr())
                {
                    This->CreateAssetManager();
                }
            });
        }
    });
}

void FGFPakExporterCommandletModule::ShutdownModule()
{
    UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("FGFPakExporterCommandletModule::ShutdownModule"))
}

FString FGFPakExporterCommandletModule::GetTempAssetRegistryPath()
{
    static const FString AssetRegistryFilename = FString(TEXT("AssetRegistry.bin")); // as per UCookOnTheFlyServer::RecordDLCPackagesFromBaseGame
    const FString PluginTempDir = FPaths::ConvertRelativePathToFull(AssetRegistryFolder.IsEmpty() ?
        FGFPakExporterModule::GetTempAssetRegistryDir() : AssetRegistryFolder);
    
    return FPaths::Combine(PluginTempDir, AssetRegistryFilename);
}

FString FGFPakExporterCommandletModule::GetTempDevelopmentAssetRegistryPath()
{
    const FString PluginTempDir = FPaths::ConvertRelativePathToFull(AssetRegistryFolder.IsEmpty() ?
         FGFPakExporterModule::GetTempAssetRegistryDir() : AssetRegistryFolder);
    // as per UCookOnTheFlyServer::RecordDLCPackagesFromBaseGame
    return FPaths::Combine(PluginTempDir, TEXT("Metadata"), GetDevelopmentAssetRegistryFilename()); 
}

bool FGFPakExporterCommandletModule::CheckCommandLineAndAdjustSettings()
{
    // Sadly, the CookOnTheFlyServer uses its own local copy of the command line which is copied before any plugin is registered,
    // so we cannot just modify the command line to add the parameters we want: they will be disregarded by the cook.
    // The cook command need to include all required parameters.
    
    FString CmdLine = FCommandLine::Get();

    // Firstly, we look for the AuroraDLC parameter and ensure it is valid
    {
        FString Value;
        if (FParse::Value(*CmdLine, *(FGFPakExporterModule::AuroraContentDLCCommandLineParameter + TEXT("=")), Value))
        {
            const FString& AuroraDLCSettingsPath = Value;
            return CheckCommandLineAndAdjustSettingsForContentDLC(CmdLine, AuroraDLCSettingsPath);
        }
        else if (FParse::Value(*CmdLine, *(FGFPakExporterModule::AuroraBaseGameCommandLineParameter + TEXT("=")), Value))
        {
            return CheckCommandLineAndAdjustSettingsForBaseGame(CmdLine, Value);
        }
    }

    CookType = EAuroraCookType::NotAuroraCook;
    UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("No Aurora Content DLC '-%s=' or BaseGame '-%s' Parameter found in the CommandLine"),
        *FGFPakExporterModule::AuroraContentDLCCommandLineParameter, *FGFPakExporterModule::AuroraBaseGameCommandLineParameter)
    return false;
}

bool FGFPakExporterCommandletModule::CheckCommandLineAndAdjustSettingsForContentDLC(const FString& CmdLine, const FString& AuroraDLCSettingsPath)
{
    UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("Found the Aurora DLC Parameter '-%s=%s' in the CommandLine"), *FGFPakExporterModule::AuroraContentDLCCommandLineParameter, *AuroraDLCSettingsPath)
        
    DLCExporterSettings = {};
    if (FPaths::FileExists(AuroraDLCSettingsPath))
    {
        TOptional<FAuroraContentDLCExporterSettings> Settings = FAuroraContentDLCExporterSettings::FromJsonSettings(AuroraDLCSettingsPath);
        if (!Settings)
        {
            UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("Unable to load the Aurora DLC Settings file '%s'"), *AuroraDLCSettingsPath)
            return false;
        }
        DLCExporterSettings = Settings.GetValue();
    }
    else
    {
        UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("The Settings File Path passed to Aurora DLC does not exist: '%s'"), *AuroraDLCSettingsPath)
        return false;
    }

    // Read the Asset Registry Folder from the -BasedOnReleaseVersionRoot= param
    if (!FParse::Value(*CmdLine, TEXT("BasedOnReleaseVersionRoot="), AssetRegistryFolder))
    {
        UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("No '-BasedOnReleaseVersionRoot=' Parameter found in the CommandLine"))
        return false;
    }
    UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("Aurora DLC temporary AssetRegistry Path: '%s'"), *AssetRegistryFolder)

    
    // Then we make sure some of the Packaging Settings are set to what we expect
    UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("Adjusting the Project Packaging Settings..."))
    UProjectPackagingSettings* PackagingSettings = GetMutableDefault<UProjectPackagingSettings>();
    if (!PackagingSettings->DirectoriesToAlwaysCook.IsEmpty())
    {
        UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT(" - DirectoriesToAlwaysCook will be emptied for this cook (contains %d directories)"), PackagingSettings->DirectoriesToAlwaysCook.Num())
        PackagingSettings->DirectoriesToAlwaysCook.Empty(); // We want to be 100% in control about the directories we cook
    }
    
    if (PackagingSettings->bUseZenStore)
    {
        UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT(" - ZenStore is not yet compatible. It will be deactivated for this cook"))
        PackagingSettings->bUseZenStore = false; // We want to be 100% in control
    }
    
    UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("Adjusting the Console Variables..."))
    if (IConsoleVariable* CookAllVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Cook.CookAllByDefault")))
    {
        if (CookAllVar->GetBool())
        {
            UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT(" - 'Cook.CookAllByDefault' will be set to false for this cook"))
            CookAllVar->Set(false);
        }
    }

    CookType = EAuroraCookType::AuroraContentDLC;
    return true;
}

bool FGFPakExporterCommandletModule::CheckCommandLineAndAdjustSettingsForBaseGame(const FString& CmdLine, const FString& AuroraSettings)
{
    UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("Found the Aurora Base Game Parameter '-%s=%s' in the CommandLine"), *FGFPakExporterModule::AuroraBaseGameCommandLineParameter, *AuroraSettings)
        
    BaseGameExporterSettings = {};
    if (FPaths::FileExists(AuroraSettings))
    {
        TOptional<FAuroraBaseGameExporterSettings> Settings = FAuroraBaseGameExporterSettings::FromJsonSettings(AuroraSettings);
        if (!Settings)
        {
            UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("Unable to load the Aurora Base Game Settings file '%s'"), *AuroraSettings)
            return false;
        }
        BaseGameExporterSettings = Settings.GetValue();
    }
    else
    {
        UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("The Settings File Path passed to Aurora Base Game does not exist: '%s'"), *AuroraSettings)
        return false;
    }
    
    CookType = EAuroraCookType::AuroraBaseGame;
    return true;
}

void FGFPakExporterCommandletModule::CreateAssetManager()
{
    UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("FGFPakExporterCommandletModule::CreateAssetManager"))
    if (!GEngine)
    {
        UE_LOG(LogGFPakExporterCommandlet, Error, TEXT("FGFPakExporterCommandletModule::CreateAssetManager:  GEngine is still null"))
        return;
    }
    
    if (GEngine->AssetManager)
    {
        UE_LOG(LogGFPakExporterCommandlet, Error, TEXT("FGFPakExporterCommandletModule::CreateAssetManager:  Another AssetManager is already registered: GEngine->AssetManager is NOT null"))
        return;
    }

    // As per regular creation of the Asset Manager in UEngine::InitializeObjectReferences()
    UGFPakExporterAssetManager* AssetManager = NewObject<UGFPakExporterAssetManager>(GEngine);
    GEngine->AssetManager = AssetManager;
    check(GEngine->AssetManager);
    if (!FParse::Param(FCommandLine::Get(), TEXT("SkipAssetScan")))
    {
        GEngine->AssetManager->StartInitialLoading();
    }

    if (CookType == EAuroraCookType::AuroraBaseGame)
    {
        AssetManager->OnModifyCookDelegate.AddLambda([](TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook)
        {
            if (FGFPakExporterCommandletModule* This = GetPtr())
            {
                TSet<FName> PackagesToAdd {FGFPakExporterModule::GetAssetDependencies(This->AdditionalPackagesToCook.Array())};
                while (PackagesToAdd.Num() > 0)
                {
                    FName PackageToAdd = *PackagesToAdd.begin();
                    PackagesToAdd.Remove(PackageToAdd);
                    if (!This->AdditionalPackagesToCook.Contains(PackageToAdd))
                    {
                        This->AdditionalPackagesToCook.Add(PackageToAdd);
                        const TArray<FName> DependentPackageNames = FGFPakExporterModule::GetAssetDependencies({PackageToAdd});
                        PackagesToAdd.Append(DependentPackageNames);
                    }
                }
                
                PackagesToCook.Append(This->AdditionalPackagesToCook.Array());
                PackagesToNeverCook.Append(This->AdditionalPackagesToNeverCook.Array());
                PackagesToNeverCook.RemoveAll([&PackagesToCook](const FName& PackageName)
                {
                    return PackagesToCook.Contains(PackageName);
                });
            }
        });
        AssetManager->OnGetPackageCookRule.BindLambda([](EPrimaryAssetCookRule CurrentCookRule, FName PackageName)
        {
            if (FGFPakExporterCommandletModule* This = GetPtr())
            {
                FAuroraBaseGameExporterConfig::EAssetExportRule ExportRule = This->BaseGameExporterSettings.Config.GetAssetExportRule(PackageName);
                if (ExportRule == FAuroraBaseGameExporterConfig::EAssetExportRule::Include || This->AdditionalPackagesToCook.Contains(PackageName))
                {
                    This->AdditionalPackagesToCook.Add(PackageName);
                    return EPrimaryAssetCookRule::AlwaysCook;
                }
                if (ExportRule == FAuroraBaseGameExporterConfig::EAssetExportRule::Exclude || This->AdditionalPackagesToNeverCook.Contains(PackageName))
                {
                    //todo: might not work properly with Cook All
                    return EPrimaryAssetCookRule::Unknown; // We cannot return `NeverCook` as the asset might be referenced by another asset.
                }
            }
            return CurrentCookRule;
        });
    }
    else if (CookType == EAuroraCookType::AuroraContentDLC)
    {
        // Add a delegate to create the Asset Registry when we are sure that the project Asset Registry is fully loaded
        AssetManager->OnModifyDLCCookDelegate.AddLambda([](const FString& DLCName, TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook)
        {
            if (FGFPakExporterCommandletModule* This = GetPtr())
            {
                TArray<FSoftObjectPath> Assets = This->CreateTemporaryAssetRegistry();
                for (const FSoftObjectPath& Asset :Assets)
                {
                    PackagesToCook.Add(Asset.GetLongPackageFName());
                }
            }
        });
    }
}

TArray<FSoftObjectPath> FGFPakExporterCommandletModule::CreateTemporaryAssetRegistry()
{
    UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("CreateTemporaryAssetRegistry"))
    TArray<FSoftObjectPath> Assets;
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    
    // As per UCookOnTheFlyServer::RecordDLCPackagesFromBaseGame, the OverrideAssetRegistry cannot be empty, so we create a copy of the project one
    FAssetRegistryState PluginAssetRegistry;
    const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    AssetRegistry.InitializeTemporaryAssetRegistryState(PluginAssetRegistry, FAssetRegistrySerializationOptions{UE::AssetRegistry::ESerializationTarget::ForDevelopment});
    
    
    // Then we list and remove the assets that should be packaged
    UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("Enumerate Assets..."))
    PluginAssetRegistry.EnumerateAllAssets({}, [this, &Assets](const FAssetData& AssetData)
    {
        if (DLCExporterSettings.Config.ShouldExportAsset(AssetData))
        {
            UE_LOG(LogGFPakExporterCommandlet, Display, TEXT(" - ShouldExportAsset: '%s'"), *AssetData.GetObjectPathString())
            Assets.AddUnique(AssetData.GetSoftObjectPath());
        }
        return true;
    });

    if (DLCExporterSettings.Config.bIncludeHardReferences)
    {
        TArray<FName> DependentPackageNames = FGFPakExporterModule::GetAssetDependencies(Assets);
        for (const FName& PackageName : DependentPackageNames)
        {
            TArrayView<const FAssetData* const> DependentAssets = PluginAssetRegistry.GetAssetsByPackageName(PackageName);
            for (const FAssetData* const Asset : DependentAssets)
            {
                Assets.AddUnique(Asset->GetSoftObjectPath());
            }
        }
    }
    
    for (const FSoftObjectPath& Asset : Assets)
    {
        bool bRemovedAssetData;
        bool bRemovedPackageData;
        PluginAssetRegistry.RemoveAssetData(Asset, true, bRemovedAssetData,bRemovedPackageData);
    }
    
    // Finally, we save the Asset Registry
    TArray<TPair<UE::AssetRegistry::ESerializationTarget, FString>> AssetRegistryOptions;
    AssetRegistryOptions.Add({
        UE::AssetRegistry::ESerializationTarget::ForDevelopment,
        GetTempDevelopmentAssetRegistryPath()}
    );
    AssetRegistryOptions.Add({
        UE::AssetRegistry::ESerializationTarget::ForGame,
        GetTempAssetRegistryPath()}
    );

    for (const TPair<UE::AssetRegistry::ESerializationTarget, FString>& Option : AssetRegistryOptions)
    {
        const FString& Path = Option.Value;
        if (!PlatformFile.CreateDirectoryTree(*FPaths::GetPath(Path)))
        {
            UE_LOG(LogGFPakExporterCommandlet, Error, TEXT("Unable to create the directory to save the AssetRegistry '%s'"), *Path)
            continue;
        }
        
        if (IFileHandle* FileHandle = PlatformFile.OpenWrite(*Path))
        {
            FArchiveFileWriterGeneric AssetRegistryWriter{FileHandle, *Path, 0};
            PluginAssetRegistry.Save(AssetRegistryWriter, FAssetRegistrySerializationOptions{Option.Key});
            UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("  Created '%s' AssetRegistry: '%s'"),
                Option.Key == UE::AssetRegistry::ESerializationTarget::ForGame ? TEXT("Game") : TEXT("Development"),
                *Path)
        }
        else
        {
            UE_LOG(LogGFPakExporterCommandlet, Error, TEXT("Unable to create '%s' AssetRegistry: '%s'"),
                Option.Key == UE::AssetRegistry::ESerializationTarget::ForGame ? TEXT("Game") : TEXT("Development"),
                *Path)
        }
    }

    return Assets;
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FGFPakExporterCommandletModule, GFPakExporterCommandlet)