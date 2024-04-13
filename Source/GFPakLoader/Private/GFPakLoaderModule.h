// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"

#include "GFPakLoaderPlatformFile.h"
#include "HAL/IPlatformFileModule.h"
#include "Modules/ModuleManager.h"

class FGFPakLoaderModule : public IPlatformFileModule
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface
	virtual IPlatformFile* GetPlatformFile() override
	{
		static TUniquePtr<IPlatformFile> AutoDestroySingleton = MakeUnique<FGFPakLoaderPlatformFile>();
		return AutoDestroySingleton.Get();
	}
};
