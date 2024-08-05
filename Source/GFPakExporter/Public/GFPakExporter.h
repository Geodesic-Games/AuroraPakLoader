// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FGFPakExporterContextMenu;

class GFPAKEXPORTER_API FGFPakExporterModule : public IModuleInterface
{
public:
    static const FName ModuleName;
    /** The Name of the command line switch added to the cook command for DLC Content. Should return 'AuroraDLCConfig' */
    static const FString AuroraContentDLCCommandLineParameter;
    /** The Name of the command line switch added to the cook command for Base Game. Should return 'AuroraBaseGameConfig' */
    static const FString AuroraBaseGameCommandLineParameter;
    
    static FGFPakExporterModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FGFPakExporterModule>(ModuleName);
    }
    static FGFPakExporterModule* GetPtr()
    {
        return FModuleManager::GetModulePtr<FGFPakExporterModule>(ModuleName);
    }
    
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /** Return the temporary directory used by this plugin which is located in '<project>/Intermediate/AuroraExporter' */
    static FString GetPluginTempDir()
    {
        return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("AuroraExporter")));
    }
    /** Return the temporary directory used by this plugin which is located in '<project>/Intermediate/AuroraExporter/AssetRegistry' */
    static FString GetTempAssetRegistryDir()
    {
        return FPaths::Combine(GetPluginTempDir(), TEXT("AssetRegistry"));
    }
    /**
     * Return the temporary StagedBuild directory folder located in '<project>/Intermediate/AuroraExporter/StagedBuilds'
     * Needed to not pollute the Plugins directory with DLCs
     * todo: path might be too long once the content is in, maybe add a Project Setting?
     */
    static FString GetTempStagingDir()
    {
        return FPaths::Combine(GetPluginTempDir(), TEXT("StagedBuilds"));
    }
    /**
     * Return the temporary Cook directory folder located in '<project>/Intermediate/AuroraExporter/Cooked'
     * Needed to not pollute the Plugins directory with DLCs
     * todo: path might be too long once the content is in, maybe add a Project Setting?
     */
    static FString GetTempCookDir()
    {
        return FPaths::Combine(GetPluginTempDir(), TEXT("Cooked"));
    }
    /** Return the list of PackageNames the given Assets are dependent on */
    static TArray<FName> GetAssetDependencies(const TArray<FSoftObjectPath>& Assets);
    static TArray<FName> GetAssetDependencies(const TArray<FName>& PackageNames);
private:
    /** Holds our context menu handler */
    TSharedPtr<FGFPakExporterContextMenu> ContentBrowserContextMenu;

    static void RegisterPropertyTypeCustomizations();
    static void UnregisterPropertyTypeCustomizations();
};
