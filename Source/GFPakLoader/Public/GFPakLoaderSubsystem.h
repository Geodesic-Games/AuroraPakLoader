// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"

#include "GFPakLoaderSettings.h"
#include "GFPakPlugin.h"
#include "Algo/Copy.h"
#include "Engine/GameInstance.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GFPakLoaderSubsystem.generated.h"

class FGFPakLoaderPlatformFile;
class FPakPlatformFile;
DECLARE_MULTICAST_DELEGATE_OneParam(FPakPluginEvent, UGFPakPlugin*);
DECLARE_MULTICAST_DELEGATE(FGFPakLoaderSubsystemEvent);

/**
 * 
 */
UCLASS()
class GFPAKLOADER_API UGFPakLoaderSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader Subsystem")
	static UGFPakLoaderSubsystem* Get()
	{
		UGFPakLoaderSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UGFPakLoaderSubsystem>() : nullptr;
		return IsValid(Subsystem) ? Subsystem : nullptr;
	}
	
	/** Implement this for initialization of instances of the system */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	/** Implement this for deinitialization of instances of the system */
	virtual void Deinitialize() override;

	FGFPakLoaderSubsystemEvent& OnSubsystemReady() { return OnSubsystemReadyDelegate; }
	FGFPakLoaderSubsystemEvent& OnSubsystemShuttingDown() { return OnSubsystemShuttingDownDelegate; }
	FGFPakLoaderSubsystemEvent& OnSubsystemShutdown() { return OnSubsystemShutdownDelegate; }
	
	FPakPluginEvent& OnPakPluginAdded() { return OnPakPluginAddedDelegate; }
	FOnStatusChanged& OnPakPluginStatusChanged() { return OnPakPluginStatusChangedDelegate; }
	FPakPluginEvent& OnPakPluginRemoved() { return OnPakPluginRemovedDelegate; }
	
	UFUNCTION(BlueprintPure, Category="GameFeatures Pak Loader Subsystem", meta=(AdvancedDisplay=1))
	FString GetDefaultPakPluginFolder() const;
	/**
	 * Look for Packaged Plugins in the given folder, add them to the subsystem and load them.
	 * This is a one time operation: if other plugins are added to the folder, the function would have to be called again.
	 * If a Packaged Plugin is already registered for this path, the function will return it.
	 * @param InPakPluginFolder The path to the folder in which to look for PakPlugins. If empty, will load the default PakPlugin Folder
	 * @param NewlyAddedPlugins The list of all the added plugins
	 * @param AllPluginsInFolder The complete list of all the plugins present in this folder.
	 */
	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader Subsystem", meta=(AdvancedDisplay=1))
	void AddPakPluginFolder(const FString& InPakPluginFolder, TArray<UGFPakPlugin*>& NewlyAddedPlugins, TArray<UGFPakPlugin*>& AllPluginsInFolder);
	/**
	 * Add a Packaged Plugin to the subsystem and load it (equivalent of calling UGFPakPluginLoader::LoadPluginData).
	 * If a Packaged Plugin is already registered for this path, the function will return it.
	 * Will return nullptr if the Path does not point to a valid Plugin Directory (UGFPakPluginLoader::LoadPluginData returned false)
	 * @param InPakPluginPath The path to the Packaged Plugin directory.
	 * We expect a packaged plugin directory to have these files and folders:
	 *	my-plugin-name/
	 *		my-plugin-name.uplugin
	 *		Content/Paks/<platform>/my-plugin-name-and-additional-things.pak
	 * Here, the path to the plugin directory would be the path to 'my-plugin-name', for example 'C:/Pak/my-plugin-name/'
	 * @param bIsNewlyAdded Returns true if the Plugin was newly added, otherwise false if it was added previously
	 * @return Returns the newly added plugins, or the existing one if the subsystem already had a plugin at this directory
	 */
	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader Subsystem", meta=(AdvancedDisplay=1))
	UGFPakPlugin* GetOrAddPakPlugin(const FString& InPakPluginPath, bool& bIsNewlyAdded);

	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader Subsystem")
	TArray<UGFPakPlugin*> GetPakPlugins() const //todo: need an enumerate function
	{
		FScopeLock Lock(&GameFeaturesPakPluginsLock);
		return GameFeaturesPakPlugins;
	}
	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader Subsystem")
	TArray<UGFPakPlugin*> GetPakPluginsWithStatus(EGFPakLoaderStatus Status) const
	{
		TArray<UGFPakPlugin*> Plugins;
		{
			FScopeLock Lock(&GameFeaturesPakPluginsLock);
			Algo::CopyIf(GameFeaturesPakPlugins, Plugins,
			   [&Status](const UGFPakPlugin* PakPlugin)
			   {
				   return IsValid(PakPlugin) && PakPlugin->GetStatus() == Status;
			   });
		}
		return Plugins;
	}
	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader Subsystem")
	TArray<UGFPakPlugin*> GetPakPluginsWithStatusAtLeast(EGFPakLoaderStatus MinStatus) const //todo: create an Enumerate function
	{
		TArray<UGFPakPlugin*> Plugins;
		{
			FScopeLock Lock(&GameFeaturesPakPluginsLock);
			Algo::CopyIf(GameFeaturesPakPlugins, Plugins,
			   [&MinStatus](const UGFPakPlugin* PakPlugin)
			   {
				   return IsValid(PakPlugin) && PakPlugin->IsStatusAtLeast(MinStatus);
			   });
		}
		return Plugins;
	}

	TSharedPtr<FPluginMountPoint> AddOrCreateMountPointFromContentPath(const FString& InContentPath);

	bool IsReady() const
	{
		return bAssetManagerCreated && bEngineLoopInitCompleted && !bIsShuttingDown;
	}
	bool IsShuttingDown() const
	{
		return bIsShuttingDown;
	}
	FGFPakLoaderPlatformFile* GetGFPakPlatformFile();

	static const UGFPakLoaderSettings* GetPakLoaderSettings()
	{
		return GetDefault<UGFPakLoaderSettings>();
	}
	static UGFPakLoaderSettings* GetMutablePakLoaderSettings()
	{
		return GetMutableDefault<UGFPakLoaderSettings>();
	}

	/**
	 * Find the Pak that should contain the given file, and optionally return the AdjustedFilename to use in the PakPlatformFile for easy retrieval.
	 * @param OriginalFilename 
	 * @param PakAdjustedFilename 
	 * @return Returns the PakPlugin in which the file should reside, otherwise null
	 */
	UGFPakPlugin* FindMountedPakContainingFile(const TCHAR* OriginalFilename, FString* PakAdjustedFilename = nullptr);
private:
	FGFPakLoaderPlatformFile* GFPakPlatformFile = nullptr;
	
	bool bAssetManagerCreated = false;
	bool bEngineLoopInitCompleted = false;
	bool bStarted = false;
	bool bIsShuttingDown = false;
	
	void OnAssetManagerCreated();
	void OnEngineLoopInitCompleted();
	void Start();

	mutable FCriticalSection GameFeaturesPakPluginsLock;
	UPROPERTY(Transient)
	TArray<UGFPakPlugin*> GameFeaturesPakPlugins;

	FGFPakLoaderSubsystemEvent OnSubsystemReadyDelegate;
	FGFPakLoaderSubsystemEvent OnSubsystemShuttingDownDelegate;
	FGFPakLoaderSubsystemEvent OnSubsystemShutdownDelegate;
	
	FOnStatusChanged OnPakPluginStatusChangedDelegate;
	FPakPluginEvent OnPakPluginAddedDelegate;
	FPakPluginEvent OnPakPluginRemovedDelegate;

	/**
	 * A Map from the Content Path on disk to the matching MountPoint.
	 * This is a static Map as there were cases where the Subsystem was deinitilialized before being reinitialized just after.
	 * When the UGFPakLoaderSubsystem deinitializes, the GFPakPlugin deactivates its GameFeature (which is asynchronous) and when done, the FPluginMountPoints unregister their MountPoints.
	 * If in between the UGFPakLoaderSubsystem is reinitialized, it will Mount all the Pak Plugins which will register their Mount Points, which will end up being unregistered once the old GameFeature
	 * is fully deactivated and the FPluginMountPoints unregister their MountPoints.
	 * This scenario can happen in Editor: play a PIE with World Partition, right-click on the WorldPartition map and `Play from here`, and do a `Play from here` a second time.
	 */
	inline static TMap<FString, TWeakPtr<FPluginMountPoint>> MountPoints = {};

	UFUNCTION()
	void PakPluginStatusChanged(UGFPakPlugin* PakPlugin, EGFPakLoaderStatus OldValue, EGFPakLoaderStatus NewValue);

	UFUNCTION()
	void PakPluginDestroyed(UGFPakPlugin* PakPlugin);

	/**
	 * Our override of IPluginManager::RegisterMountPointDelegate allowing us to stop the creation of wrong MountPoints for our PakPlugins.
	 * See UGFPakLoaderSubsystem::Initialize for more details
	 */
	void RegisterMountPoint(const FString& RootPath, const FString& ContentPath);

	TMap<FString, FString> PathToPluginPath;
	// only used to not debug print
	TSet<FString> IgnoredPluginPaths;

public: // Debug Functions
	/**
	 * Print in the log the value of the Platform Paths, as they might differ on different configs and platforms
	 */
	static void Debug_LogPaths();

	UFUNCTION()
	void OnContentPathMounted(const FString& AssetPath, const FString& ContentPath);
	UFUNCTION()
	void OnContentPathDismounted(const FString& AssetPath, const FString& ContentPath);
};
