// Copyright GeoTech BV

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class IGFPakLoaderEditorModule : public IModuleInterface
{
public:
    static inline IGFPakLoaderEditorModule& Get()
    {
        return FModuleManager::LoadModuleChecked<IGFPakLoaderEditorModule>("GFPakLoaderEditor");
    }
};
