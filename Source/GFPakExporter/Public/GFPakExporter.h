#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FGFPakExporterAssetFolderContextMenu;

class FGFPakExporterModule : public IModuleInterface
{
public:
    static inline FGFPakExporterModule& Get()
    {
        static const FName ModuleName = TEXT("GFPakLoaderExporter");
        return FModuleManager::LoadModuleChecked<FGFPakExporterModule>(ModuleName);
    }
    
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    /** Holds our context menu handler */
    TSharedPtr<FGFPakExporterAssetFolderContextMenu> AssetFolderContextMenu;
};
