#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class GFPAKEXPORTERCOMMANDLET_API FGFPakExporterCommandletModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

	static const FString ModuleName;
	static const FString PluginName;
	
	static FString GetPluginDir();
	static FString GetPluginTempDir();

	static FString GetTempAssetRegistryPath();
	static FString GetTempDevelopmentAssetRegistryPath();
private:
    /**
     * Function called to create our own Asset Manager before UE creates its own
     */
    void CreateAssetManager();
	
    FDelegateHandle OnRegisterEngineElementsHandle;

	void CreateAndCleanTempDirectory();
	void CreateDummyAssetRegistry();
};
