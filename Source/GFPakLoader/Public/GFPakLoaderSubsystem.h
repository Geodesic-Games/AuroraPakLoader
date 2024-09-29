// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"
#include "GFPakLoaderSettings.h"
#include "GFPakPlugin.h"
#include "Algo/Copy.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Subsystems/EngineSubsystem.h"
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
	/**
	 * Called after OnSubsystemReady once the Startup Paks have ben added.
	 * See UGFPakLoaderSettings::bAddPakPluginsFromStartupLoadDirectory and GetDefaultPakPluginFolder
	 */
	FGFPakLoaderSubsystemEvent& OnStartupPaksAdded() { return OnStartupPaksAddedDelegate; }
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
	TArray<UGFPakPlugin*> GetPakPlugins() const
	{
		FRWScopeLock Lock(GameFeaturesPakPluginsLock, SLT_ReadOnly);
		return GameFeaturesPakPlugins;
	}
	/** Return the list of PakPlugins sorted by their GetSafePluginName. Slower than GetPakPlugins but useful for UI */
	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader Subsystem")
	TArray<UGFPakPlugin*> GetSortedPakPlugins()
	{
		TArray<UGFPakPlugin*> Plugins;
		{
			FRWScopeLock Lock(GameFeaturesPakPluginsLock, SLT_ReadOnly);
			Plugins = GameFeaturesPakPlugins;
		}
		Plugins.Sort([](const UGFPakPlugin& Lhs, const UGFPakPlugin& Rhs)
		{
			return Lhs.GetSafePluginName() < Rhs.GetSafePluginName();
		});
		return Plugins;
	}

	enum class EComparison : uint8
	{
		Equal,
		NotEqual,
		Greater,
		Less,
		GreaterOrEqual,
		LessOrEqual,
	};
	enum class EForEachResult : uint8
	{
		Continue,
		Break,
	};
	
	/**
	 * Enumerate all the PakPlugins registered with the subsystem.
	 * @param Callback Callback to be called on each PakPlugin. It is guaranteed that the PakPlugin is a valid UObject
	 */
	void EnumeratePakPlugins(TFunctionRef<EForEachResult(UGFPakPlugin* PakPlugin)> Callback)
	{
		FRWScopeLock Lock(GameFeaturesPakPluginsLock, SLT_ReadOnly);
		for (UGFPakPlugin* PakPlugin : GameFeaturesPakPlugins)
		{
			if (IsValid(PakPlugin))
			{
				if (Callback(PakPlugin) == EForEachResult::Break)
				{
					break;
				}
			}
		}
	}
	/**
	 * Enumerate all the PakPlugins registered with the subsystem.
	 * @param Predicate Predicate that must return true for the Callback to be run on the given PakPlugin.
	 * @param Callback Callback to be called on each PakPlugin. It is guaranteed that the PakPlugin is a valid UObject
	 */
	void EnumeratePakPlugins(TFunctionRef<bool(const UGFPakPlugin* PakPlugin)> Predicate, TFunctionRef<EForEachResult(UGFPakPlugin* PakPlugin)> Callback)
	{
		FRWScopeLock Lock(GameFeaturesPakPluginsLock, SLT_ReadOnly);
		for (UGFPakPlugin* PakPlugin : GameFeaturesPakPlugins)
		{
			if (IsValid(PakPlugin))
			{
				if (!Predicate(PakPlugin))
				{
					continue;
				}
				
				if (Callback(PakPlugin) == EForEachResult::Break)
				{
					break;
				}
			}
		}
	}
	
	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader Subsystem")
	TArray<UGFPakPlugin*> GetPakPluginsWithStatusEqualTo(EGFPakLoaderStatus Status)
	{
		return GetPakPluginsWithStatus<EComparison::Equal>(Status);
	}
	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader Subsystem")
	TArray<UGFPakPlugin*> GetPakPluginsWithStatusGreaterOrEqualTo(EGFPakLoaderStatus MinStatus)
	{
		return GetPakPluginsWithStatus<EComparison::GreaterOrEqual>(MinStatus);
	}
	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader Subsystem")
	TArray<UGFPakPlugin*> GetPakPluginsWithStatusLessOrEqualTo(EGFPakLoaderStatus MaxStatus)
	{
		return GetPakPluginsWithStatus<EComparison::LessOrEqual>(MaxStatus);
	}
	

	/**
	 * Enumerate all the PakPlugins registered with the subsystem with their Status matching the Status and Comparison.
	 * @param Callback Callback to be called on each PakPlugin. It is guaranteed that the PakPlugin is a valid UObject. Return true to break out of the loop.
	 */
	template <EComparison Comparison>
	void EnumeratePakPluginsWithStatus(EGFPakLoaderStatus Status, TFunctionRef<EForEachResult(UGFPakPlugin* PakPlugin)> Callback)
	{
		auto Predicate =[Status](const UGFPakPlugin* PakPlugin)
		{
			if constexpr (Comparison == EComparison::Equal)
			{
				return PakPlugin->GetStatus() == Status;
			}
			else if constexpr (Comparison == EComparison::NotEqual)
			{
				return PakPlugin->GetStatus() != Status;
			}
			else if constexpr (Comparison == EComparison::GreaterOrEqual)
			{
				return PakPlugin->GetStatus() >= Status;
			}
			else if constexpr (Comparison == EComparison::Greater)
			{
				return PakPlugin->GetStatus() > Status;
			}
			else if constexpr (Comparison == EComparison::LessOrEqual)
			{
				return PakPlugin->GetStatus() <= Status;
			}
			else if constexpr (Comparison == EComparison::Less)
			{
				return PakPlugin->GetStatus() < Status;
			}
			else
			{
				return false;
			}
		};
		
		EnumeratePakPlugins(Predicate, Callback);
	}
	
	template <EComparison Comparison>
	TArray<UGFPakPlugin*> GetPakPluginsWithStatus(EGFPakLoaderStatus Status)
	{
		TArray<UGFPakPlugin*> Plugins;
		EnumeratePakPluginsWithStatus<Comparison>(Status, [&Plugins](UGFPakPlugin* PakPlugin)
		{
			Plugins.Add(PakPlugin);
			return EForEachResult::Continue;
		});
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
	void PreLoadMapWithContext(const FWorldContext& WorldContext, const FString& MapName);
	void Start();

	mutable FRWLock GameFeaturesPakPluginsLock;
	UPROPERTY(Transient)
	TArray<UGFPakPlugin*> GameFeaturesPakPlugins;

	FGFPakLoaderSubsystemEvent OnSubsystemReadyDelegate;
	FGFPakLoaderSubsystemEvent OnStartupPaksAddedDelegate;
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
	
	// only used to not spam the log with repeating errors
	TSet<FString> IgnoredPluginPaths;
	TSet<FString> InvalidDirectories;

	
	FCriticalSection AssetOwnerMutex;
	struct FAssetOwner
	{
		bool bIsBaseGameAsset = false;
		FAssetData BaseGameAssetData;
		TArray<UGFPakPlugin*> PluginOwners; // Not yet really needed, but will be useful once we tackle which Pak Plugin has priority
	};
	// Keep track of assets added by the DLC Paks and previously existing assets
	TMap<FSoftObjectPath, FAssetOwner> AssetOwners;
	
	friend UGFPakPlugin;
	/** Function to ensure the Pak assets added to the asset registry are recorded as we might be "overriding" some existing ones */
	void OnPreAddPluginAssetRegistry(const FAssetRegistryState& PluginAssetRegistry, UGFPakPlugin* Plugin);
	/** Function to remove the Pak assets from the asset registry while keeping the existing ones */
	void OnPreRemovePluginAssetRegistry(const FAssetRegistryState& PluginAssetRegistry, UGFPakPlugin* Plugin, TSet<FName>& OutPackageNamesToRemove);

public: // Debug Functions
	/** Print in the log the value of the Platform Paths, as they might differ on different configs and platforms */
	static void Debug_LogPaths();

	UFUNCTION()
	void OnContentPathMounted(const FString& AssetPath, const FString& ContentPath);
	UFUNCTION()
	void OnContentPathDismounted(const FString& AssetPath, const FString& ContentPath);
	
	void OnEnsureWorldIsLoadedInMemoryBeforeLoadingMapChanged();

	/** Helper function to return a string describing the PackageFlags */
	static FString PackageFlagsToString(uint32 PackageFlags);
};
