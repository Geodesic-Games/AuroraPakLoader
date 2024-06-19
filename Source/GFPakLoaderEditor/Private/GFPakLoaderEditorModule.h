// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "IGFPakLoaderEditorModule.h"

class FGFPakLoaderEditorModule : public IGFPakLoaderEditorModule
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	/** The name of the folder used in the Content Browser for Pak Plugins*/
	static const FString GFPakLoaderVirtualPathPrefix;
	static void OnContentBrowserGenerateVirtualPathPrefix(const FStringView InPath, FStringBuilderBase& OutPath);
};
