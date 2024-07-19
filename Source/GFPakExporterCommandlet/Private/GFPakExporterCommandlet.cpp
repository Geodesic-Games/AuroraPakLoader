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
        FString AuroraDLCConfig;
        if (!FParse::Value(*CmdLine, *(FGFPakExporterModule::AuroraCommandLineParameter + TEXT("=")), AuroraDLCConfig))
        {
            UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("No Aurora DLC Parameter '-%s=' found in the CommandLine"), *FGFPakExporterModule::AuroraCommandLineParameter)
            return false;
        }
        UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("Found the Aurora DLC Parameter '-%s=%s' in the CommandLine"), *FGFPakExporterModule::AuroraCommandLineParameter, *AuroraDLCConfig)
        
        ExporterConfig = {};
        if (FPaths::FileExists(AuroraDLCConfig))
        {
            TOptional<FAuroraExporterConfig> Config = FAuroraExporterConfig::FromJsonConfig(AuroraDLCConfig);
            if (!Config)
            {
                UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("Unable to load the Aurora DLC Config file '%s'"), *AuroraDLCConfig)
                return false;
            }
            ExporterConfig = Config.GetValue();
        }
        else
        {
            TOptional<FAuroraExporterConfig> Config = FAuroraExporterConfig::FromPluginName(AuroraDLCConfig);
            if (!Config)
            {
                UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("The Plugin Name passed to Aurora DLC is not a valid plugin name: '%s'"), *AuroraDLCConfig)
                return false;
            }
            ExporterConfig = Config.GetValue();
        }

        // Read the Asset Registry Folder from the -BasedOnReleaseVersionRoot= param
        if (!FParse::Value(*CmdLine, TEXT("BasedOnReleaseVersionRoot="), AssetRegistryFolder))
        {
            UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("No '-BasedOnReleaseVersionRoot=' Parameter found in the CommandLine"))
            return false;
        }
        UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("Aurora DLC temporary AssetRegistry Path: '%s'"), *AssetRegistryFolder)
    }

    // Then we make sure some of the Packaging Settings are set to what we expect
    {
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
    }
    
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
        if (ExporterConfig.ShouldExportAsset(AssetData))
        {
            UE_LOG(LogGFPakExporterCommandlet, Display, TEXT(" - ShouldExportAsset: '%s'"), *AssetData.GetObjectPathString())
            Assets.AddUnique(AssetData.GetSoftObjectPath());
        }
        return true;
    });

    if (ExporterConfig.bIncludeHardReferences)
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