// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FGFPakExporterContentBrowserContextMenu;

class GFPAKEXPORTER_API FGFPakExporterModule : public IModuleInterface
{
public:
    static const FName ModuleName;
    /** The Name of the command line switch added to the cook command. Should return 'AuroraDLCConfig' */
    static const FString AuroraCommandLineParameter;
    
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
    static FString GetPluginTempDir();
    /** Return the temporary directory used by this plugin which is located in '<project>/Intermediate/AuroraExporter/AssetRegistry' */
    static FString GetTempAssetRegistryDir();
private:
    /** Holds our context menu handler */
    TSharedPtr<FGFPakExporterContentBrowserContextMenu> ContentBrowserContextMenu;
};
