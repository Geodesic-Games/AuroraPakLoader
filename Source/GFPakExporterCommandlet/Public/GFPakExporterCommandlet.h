#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "AuroraExporterSettings.h"

class GFPAKEXPORTERCOMMANDLET_API FGFPakExporterCommandletModule : public IModuleInterface
{
public:
	static FGFPakExporterCommandletModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FGFPakExporterCommandletModule>(ModuleName);
	}
	static FGFPakExporterCommandletModule* GetPtr()
	{
		return FModuleManager::GetModulePtr<FGFPakExporterCommandletModule>(ModuleName);
	}
	
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

	static const FName ModuleName;
	static const FString PluginName;

	/** Return the temporary AssetRegistry used by this plugin which is located in '<project>/Intermediate/AuroraExporter/AssetRegistry/AssetRegistry.bin' */
	FString GetTempAssetRegistryPath();
	/** Return the temporary Development AssetRegistry used by this plugin which is located in '<project>/Intermediate/AuroraExporter/AssetRegistry/Metadata/AssetRegistry.bin' */
	FString GetTempDevelopmentAssetRegistryPath();

	enum class EAuroraCookType : uint8
	{
		Unknown,
		NotAuroraCook,
		AuroraContentDLC,
		AuroraBaseGame,
	};
	EAuroraCookType GetCookType() const { return CookType; }
private:
	/** Adjust the command line as necessary. Returns true if this cook is an Aurora DLC cook (meaning having a FGFPakExporterModule::AuroraCommandLineSwitch flag in the command line) */
	bool CheckCommandLineAndAdjustSettings();
	bool CheckCommandLineAndAdjustSettingsForContentDLC(const FString& CmdLine, const FString& AuroraDLCSettingsPath);
	bool CheckCommandLineAndAdjustSettingsForBaseGame(const FString& CmdLine, const FString& AuroraSettings);

	/** Function called to create our own Asset Manager before UE creates its own */
	void CreateAssetManager();
	/**
	 * Create a new Asset Registry (based on the project one) to be used as basis for the cook.
	 * Return the Assets that have been removed from the Asset Registry copy and that should therefore be part of the final Pak
	 */
	TArray<FSoftObjectPath> CreateTemporaryAssetRegistry();

	/** The Aurora Settings read for this DLC cook */
	FAuroraContentDLCExporterSettings DLCExporterSettings;
	/** The Aurora Settings read for this DLC cook */
	FAuroraBaseGameExporterSettings BaseGameExporterSettings;
	/**
	 * The Folder containing the Dummy Asset Registry. It is read from the commandline parameter
	 * -BasedOnReleaseVersionRoot and should normally be set to FGFPakExporterModule::GetTempAssetRegistryDir()
	 */
	FString AssetRegistryFolder;

	EAuroraCookType CookType = EAuroraCookType::Unknown;

	TSet<FName> AdditionalPackagesToCook;
	TSet<FName> AdditionalPackagesToNeverCook;
};
