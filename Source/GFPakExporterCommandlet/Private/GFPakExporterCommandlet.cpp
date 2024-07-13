#include "GFPakExporterCommandlet.h"

#include "AssetToolsModule.h"
#include "FileHelpers.h"
#include "GameDelegates.h"
#include "GFPakExporterAssetManager.h"
#include "GFPakExporterCommandletLog.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "Elements/Framework/EngineElements.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "HAL/FileManagerGeneric.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Settings/ProjectPackagingSettings.h"

#define LOCTEXT_NAMESPACE "FGFPakExporterCommandletModule"

const FString FGFPakExporterCommandletModule::PluginName{TEXT("AuroraPakLoader")};
const FString FGFPakExporterCommandletModule::ModuleName{TEXT("GFPakExporterCommandlet")};

void FGFPakExporterCommandletModule::StartupModule() //todo: called too late, need to be done earlier that PreInitPostStartupScreen
{
    UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("FGFPakExporterCommandletModule::StartupModule"))
    if (!IsRunningCommandlet())
    {
        UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("FGFPakExporterCommandletModule::StartupModule:  Not running a commandlet"))
        // FModuleManager::Get().UnloadModule(FName{ModuleName});
        return;
    }
    
    UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("FGFPakExporterCommandletModule::StartupModule: Running a commandlet"))
    
    FString CmdLine = FCommandLine::Get();

    FString AuroraDLCPlugin;
    if (!FParse::Value(*CmdLine, TEXT("-AuroraDLCPlugin="), AuroraDLCPlugin))
    {
        UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("No Aurora DLC Token '-AuroraDLCPlugin=' found in the CommandLine"))
        return;
    }
    UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("Found the Aurora DLC Token '-AuroraDLCPlugin=%s' in the CommandLine"), *AuroraDLCPlugin)
    
    FString AssetRegistryFolder = FPaths::GetPath(GetTempAssetRegistryPath());
    TMap<FString, FString> OverrideSwitches {
        {TEXT("DLCName"), AuroraDLCPlugin}, // Name of the DLC
        {TEXT("BasedOnReleaseVersion"), TEXT("AuroraDummyReleaseVersion")}, // Dummy value
        {TEXT("BasedOnReleaseVersionRoot"), FString::Printf(TEXT("\"%s\""),*AssetRegistryFolder)}, // Folder containing our custom Asset Registry
        {TEXT("DevelopmentAssetRegistryPlatformOverride"), TEXT("../")}, // Need to be the parent folder to disregard the BasedOnReleaseVersion
        {TEXT("DLCIncludeEngineContent"), TEXT("")}, // We want the Engine content to be listed in our callbacks
        //todo: these flags cannot currently be modified due to how the cook commandlet checks the command line arguments, so put a warning if they are not set
        {TEXT("CookSkipRequests"), TEXT("")}, // We don't want StartupPackages by default
        {TEXT("CookSkipSoftRefs"), TEXT("")}, // We don't want Startup Soft References by default 
        {TEXT("CookSkipHardRefs"), TEXT("")}, // We don't want Startup Hard References by default
        {TEXT("DisableUnsolicitedPackages"), TEXT("")}, // should do the same as the above two
        {TEXT("NoGameAlwaysCookPackages"), TEXT("")}, // We don't want the DirectoriesToAlwaysCook packages
        {TEXT("SkipZenStore"), TEXT("")}, // We don't want the ZenStore at this stage
        {TEXT("dummy"), TEXT("")}, // We want the Engine content to be listed in our callbacks
    };
    
    TArray<FString> Tokens;
    TArray<FString> Switches;
    FCommandLine::Parse(*CmdLine, Tokens, Switches);
    Switches.RemoveAll([&Tokens, &OverrideSwitches](const FString& Switch)
    {
        Tokens.Remove(Switch);
        FString Left;
        FString Right;
        const FString& SwitchKey = Switch.Split(TEXT("="), &Left, &Right) ? Left : Switch;
        if (OverrideSwitches.Contains(SwitchKey))
        {
            UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT("Overriding the Command Line switch '-%s' from '%s' to '%s'"), *SwitchKey, *Right, *OverrideSwitches[SwitchKey])
            return true;
        }
        return false;
    });
    for (auto& OverrideSwitch : OverrideSwitches)
    {
        Switches.Add(OverrideSwitch.Key + (OverrideSwitch.Value.IsEmpty() ? TEXT("") : TEXT("=") + OverrideSwitch.Value));
    }
    FString AdjustedCommandLine = FString::Join(Tokens, TEXT(" ")) + TEXT(" ") + FString::JoinBy(Switches, TEXT(" "), [](const FString& Str){ return TEXT("-") + Str; });
    FCommandLine::Set(*AdjustedCommandLine);
    UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("CommandLine adjusted\nfrom:\n%s\nto:\n%s"), *CmdLine, *AdjustedCommandLine)
    
    // CreateAndCleanTempDirectory();
    // CreateDummyAssetRegistry();
    
    OnRegisterEngineElementsHandle = OnRegisterEngineElementsDelegate.AddLambda([]()
    {
        UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("OnRegisterEngineElementsDelegate"))
        if (FGFPakExporterCommandletModule* This = FModuleManager::Get().GetModulePtr<FGFPakExporterCommandletModule>(FName{ModuleName}))
        {
            UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("Adjusting the Project Packaging Settings..."))
            UProjectPackagingSettings* PackagingSettings = GetMutableDefault<UProjectPackagingSettings>();
            if (!PackagingSettings->DirectoriesToAlwaysCook.IsEmpty())
            {
                UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT(" - DirectoriesToAlwaysCook will be emptied for this cook (contains %d directories)"), PackagingSettings->DirectoriesToAlwaysCook.Num())
                PackagingSettings->DirectoriesToAlwaysCook.Empty(); // We want to be 100% in control about the directories we cook
            }
            // if (PackagingSettings->bUseZenStore)
            // {
            //     UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT(" - ZenStore is not yet compatible. It will be deactivated for this cook"))
            //     PackagingSettings->bUseZenStore = false; // We want to be 100% in control
            // }
            UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("Adjusting the Console Variables..."))
            if (IConsoleVariable* CookAllVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Cook.CookAllByDefault")))
            {
                if (CookAllVar->GetBool())
                {
                    UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT(" - 'Cook.CookAllByDefault' will be set to false for this cook"))
                    CookAllVar->Set(false);
                }
            }
            
            
            // This->CreateAssetManager();
            FGameDelegates::Get().GetModifyCookDelegate().AddLambda([](TConstArrayView<const ITargetPlatform*> InTargetPlatforms, TArray<FName>& InOutPackagesToCook, TArray<FName>& InOutPackagesToNeverCook)
            {
                UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("FGameDelegates::GetModifyCookDelegate"))
                if (FGFPakExporterCommandletModule* This = FModuleManager::Get().GetModulePtr<FGFPakExporterCommandletModule>(FName{ModuleName}))
                {
                    This->CreateDummyAssetRegistry();
                }
            });
        }
    });
    // FCommandLine::Get()
    
    
    //todo: can we modify the commandline?
    // goal would be to pass one vague command like `-AuroraDLCPlugin=Style02`, and then add the other necessary commands
    // like `-BasedOnReleaseVersionRoot`, `-basedonreleaseversion`, `-DevelopmentAssetRegistryPlatformOverride`, etc
}

void FGFPakExporterCommandletModule::CreateAndCleanTempDirectory()
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    
    // Ensure the directories exist
    check(FPaths::DirectoryExists(GetPluginDir()));
    FString PluginTempDir = FPaths::ConvertRelativePathToFull(GetPluginTempDir());
    UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("CreateAndCleanTempDirectory:  Temp directory: '%s'"), *PluginTempDir)

    if (FPaths::DirectoryExists(PluginTempDir))
    {
        PlatformFile.DeleteDirectoryRecursively(*PluginTempDir);
        UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT("Deleted the diretory recursively:  '%s'"), *PluginTempDir)
    }

    if (PlatformFile.CreateDirectoryTree(*PluginTempDir))
    {
        UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT("Created the diretory:  '%s'"), *PluginTempDir)
    }
    else
    {
        UE_LOG(LogGFPakExporterCommandlet, Fatal, TEXT("Unable to create the directory '%s'"), *PluginTempDir)
    }
}

void FGFPakExporterCommandletModule::CreateDummyAssetRegistry()
{
    UE_LOG(LogGFPakExporterCommandlet, Display, TEXT("CreateDummyAssetRegistry"))

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    
    // As per UCookOnTheFlyServer::RecordDLCPackagesFromBaseGame, the OverrideAssetRegistry cannot be empty, so we create one with a dummy asset
    FAssetRegistryState PluginAssetRegistry;
    {
        // FString PackageNameDirStr(TEXT("/FGFPakExporterCommandletModule")); // Needs to be in an existing MountPoint, so reuse Engine
        // FString ContentFolder = GetPluginTempDir() / TEXT("Content");
        // if (!PlatformFile.CreateDirectoryTree(*ContentFolder))
        // {
        //     UE_LOG(LogGFPakExporterCommandlet, Fatal, TEXT("Unable to create the content folder '%s'"), *ContentFolder)
        // }
        //
        // FPackageName::RegisterMountPoint(PackageNameDirStr + TEXT("/"), ContentFolder);
        //
        // FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
        // FString BaseAssetName{TEXT("DummyPackage")};
        // FString PackageName;
        // FString AssetName;
        // AssetToolsModule.Get().CreateUniqueAssetName(PackageNameDirStr + TEXT("/") + BaseAssetName, TEXT(""), /*out*/ PackageName, /*out*/ AssetName);
        // const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);
        //
        // UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
        // if (UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, UMaterialInstanceConstant::StaticClass(), Factory))
        // {
        //     bool bResult = UEditorLoadingAndSavingUtils::SavePackages({NewAsset->GetPackage()}, false);
        //     UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT("Dummy Asset '%s' created:  %s"), *GetFullNameSafe(NewAsset), bResult ? TEXT("Saved") : TEXT("Failed Saving"))
        //     FAssetData* AssetData = new FAssetData(NewAsset);
        //     PluginAssetRegistry.AddAssetData(AssetData);
        // }
        
        {
            const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
            IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
            // AssetRegistry.AppendState(PluginAssetRegistry); // Need to add our Dummy package to the asset registry
            AssetRegistry.InitializeTemporaryAssetRegistryState(PluginAssetRegistry, FAssetRegistrySerializationOptions{UE::AssetRegistry::ESerializationTarget::ForDevelopment});
        }
        {
            FARCompiledFilter Filter;
            TArray<FSoftObjectPath> Assets;
            FName DLCFName{TEXT("Style02")};
            Filter.PackagePaths.Add(DLCFName);
            PluginAssetRegistry.EnumerateAllAssets({}, [&Assets, &DLCFName](const FAssetData& AssetData)
            {
                FName MountPoint = FPackageName::GetPackageMountPoint(AssetData.PackagePath.ToString());
                if (MountPoint == DLCFName)
                {
                    UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("Enumerate: '%s'  => '%s'"), *AssetData.PackagePath.ToString(), *MountPoint.ToString())
                    Assets.Add(AssetData.GetSoftObjectPath());
                }
                return true;
            });
            for (FSoftObjectPath& AssetData : Assets)
            {
                bool bRemovedAssetData;
                bool bRemovedPackageData;
                PluginAssetRegistry.RemoveAssetData(AssetData, false, bRemovedAssetData,bRemovedPackageData);
            }
            
            
            // bool bResult = PluginAssetRegistry.EnumerateAssets(Filter, {}, [&Assets](const FAssetData& AssetData)
            // {
            //     FName MountPoint = FPackageName::GetPackageMountPoint(AssetData.PackagePath.ToString());
            //     UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("Enumerate: '%s'  => '%s'"), *AssetData.PackagePath.ToString(), *MountPoint.ToString())
            //     return true;
            // });
            // PluginAssetRegistry.RemoveAssetData()
            // FString Filename;
            // if (FPackageName::DoesPackageExist(AssetData.PackageName.ToString(), &Filename))
            // {
            //     if (MountPoint != DLCFName)
            //     {
            //         // PlatformBasedPackages.Add(FName{Filename});
            //         UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT(" - [Add Base]:  '%s'"), *AssetData.PackageName.ToString())
            //     }
            //     else
            //     {
            //         PackagesToClearResults.Add(FName{Filename});
            //         UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT(" - [Add Clear]:  '%s'"), *AssetData.PackageName.ToString())
            //     }
            // }
            // return true;
        }
    }
    
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
            UE_LOG(LogGFPakExporterCommandlet, Verbose, TEXT("  Created '%s' AssetRegistry: '%s'"),
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
}

FString FGFPakExporterCommandletModule::GetTempAssetRegistryPath()
{
    static const FString AssetRegistryFilename = FString(TEXT("AssetRegistry.bin")); // as per UCookOnTheFlyServer::RecordDLCPackagesFromBaseGame
    const FString PluginTempDir = FPaths::ConvertRelativePathToFull(GetPluginTempDir());
    
    return FPaths::Combine(PluginTempDir, AssetRegistryFilename);
}

FString FGFPakExporterCommandletModule::GetTempDevelopmentAssetRegistryPath()
{
    const FString PluginTempDir = FPaths::ConvertRelativePathToFull(GetPluginTempDir());
    // as per UCookOnTheFlyServer::RecordDLCPackagesFromBaseGame
    return FPaths::Combine(PluginTempDir, TEXT("Metadata"), GetDevelopmentAssetRegistryFilename()); 
}

void FGFPakExporterCommandletModule::ShutdownModule()
{
    UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("FGFPakExporterCommandletModule::ShutdownModule"))
    OnRegisterEngineElementsDelegate.Remove(OnRegisterEngineElementsHandle);
}

FString FGFPakExporterCommandletModule::GetPluginDir()
{
    if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName))
    {
        return Plugin->GetBaseDir();
    }
    return FString{};
}

FString FGFPakExporterCommandletModule::GetPluginTempDir()
{
    FString PluginDir = GetPluginDir();
    if (!PluginDir.IsEmpty())
    {
        return FPaths::Combine(PluginDir, "Temp");
    }
    return FString{};
}

void FGFPakExporterCommandletModule::CreateAssetManager()
{
    UE_LOG(LogGFPakExporterCommandlet, Warning, TEXT("FGFPakExporterCommandletModule::CreateAssetManager"))
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
    
    
    // UClass* SingletonClass = nullptr;
    // if (AssetManagerClassName.ToString().Len() > 0)
    // {
    //     SingletonClass = LoadClass<UObject>(nullptr, *AssetManagerClassName.ToString());
    // }
    // if (!SingletonClass)
    // {
    //     UE_LOG(LogEngine, Fatal, TEXT("Engine config value AssetManagerClassName '%s' is not a valid class name."), *AssetManagerClassName.ToString());
    // }

    GEngine->AssetManager = NewObject<UGFPakExporterAssetManager>(GEngine);
    check(GEngine->AssetManager);
    if (!FParse::Param(FCommandLine::Get(), TEXT("SkipAssetScan")))
    {
        GEngine->AssetManager->StartInitialLoading();
    }
   
    OnRegisterEngineElementsDelegate.Remove(OnRegisterEngineElementsHandle);
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FGFPakExporterCommandletModule, GFPakExporterCommandlet)