﻿// Copyright GeoTech BV

#pragma once

#include "GameFeaturePluginOperationResult.h"
#include "PluginDescriptor.h"
#include "PluginMountPoint.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "GFPakPlugin.generated.h"


class UGFPakLoaderSubsystem;

UENUM(BlueprintType)
enum class EGFPakLoaderStatus : uint8
{
	NotInitialized			= 0,
	InvalidPluginDirectory	= 1,
	Unmounted				= 2,
	Mounted					= 3,
	DeactivatingGameFeature	= 4,
	ActivatingGameFeature	= 5,
	GameFeatureActivated	= 6,
};

struct FGFPakFilenameMap : TSharedFromThis<FGFPakFilenameMap>
{
#if WITH_EDITOR // Only used for debugging
	// The original mount point. ex: "../../../DLCTestProject/" or "../../../"
	FString OriginalMountPoint;
	// The adjusted mount point. ex: "/../../../DLCTestProject/" or "/../../../"
	FString AdjustedMountPoint;
	// The original filename. ex: "Content/DLCTestProjectContent/BP_DLCTestProject.uasset"
	FString OriginalFilename;
	// The combined original OriginalMountPoint and filename. ex: "../../../DLCTestProject/Content/DLCTestProjectContent/BP_DLCTestProject.uasset"
	FString OriginalFullFilename;
#endif
	/**
	 * This is the path that will be used to find the file with the PakPlatformFile.
	 * It is the combined original AdjustedMountPoint and filename. ex: "/../../../DLCTestProject/Content/DLCTestProjectContent/BP_DLCTestProject.uasset"
	 */
	FString AdjustedFullFilename;
	// The OriginalFullFilename where the project name (here "DLCTestProject") has been adjusted to the current project (FPlatformMisc::ProjectDir()), and the path made relative to FPlatformProcess::BaseDir(). ex: "../../Content/DLCTestProjectContent/BP_DLCTestProject.uasset"
	FString ProjectAdjustedFullFilename;
	// The MountedPackageName retrieved with FPackagePath::TryFromMountedName.GetPackageFName() ex: "/Game/DLCTestProjectContent/BP_DLCTestProject"
	FName MountedPackageName;
	// The MountedPackageName retrieved with MountedPackagePath.GetLocalFullPath() ex: "../../../../UnrealEngine/../Folder/Project/Content/DLCTestProjectContent/BP_DLCTestProject.uasset"
	FName LocalBaseFilename;

	static FGFPakFilenameMap FromFilenameAndMountPoints(const FString& MountPoint, const FString& OriginalFilename)
	{
		return FromFilenameAndMountPoints(MountPoint, MountPoint, OriginalFilename);
	}
	static FGFPakFilenameMap FromFilenameAndMountPoints(const FString& OriginalMountPoint, const FString& AdjustedMountPoint, const FString& OriginalFilename);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnStatusChanged, class UGFPakPlugin*, PakPlugin, EGFPakLoaderStatus, OldStatus, EGFPakLoaderStatus, NewStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPluginEvent, class UGFPakPlugin*, PakPlugin);

DECLARE_DELEGATE_TwoParams(FOperationCompleted, bool /*bSuccessful*/, const TOptional<UE::GameFeatures::FResult>& /*Result*/);

/**
 * The class responsible for loading, mounting and unmounting a Pak Plugin.
 * We expect a Pak Plugin directory to have these files and folders:
 *	my-plugin-name/
 *		my-plugin-name.uplugin
 *		Content/Paks/<platform>/my-plugin-name-and-additional-things.pak
 */
UCLASS(Blueprintable, Within = GFPakLoaderSubsystem)
class GFPAKLOADER_API UGFPakPlugin : public UObject
{
	GENERATED_BODY()
public:
	virtual void BeginDestroy() override;
	
	FPluginEvent& OnDeactivatingGameFeatures() { return OnDeactivatingGameFeaturesDelegate; }
	FPluginEvent& OnUnmounting() { return OnUnmountingDelegate; }
	FPluginEvent& OnDeinitializing() { return OnDeinitializingDelegate; }
	
	FOnStatusChanged& OnStatusChanged() { return OnStatusChangedDelegate; }
	
	FPluginEvent& OnBeginDestroy() { return OnBeginDestroyDelegate; }
	FPluginEvent& OnDestroyed() { return OnDestroyedDelegate; }

	/**
	 * Loads the Plugin Data and ensures the directory points to a valid Pak Plugin. Should be called after SetPakPluginDirectory.
	 * If successful, the Status of this instance will change to  `Unmounted`, unless the plugin data was already loaded in which case the Status will not change.
	 * @return Returns true if we were able to load the plugin data and the plugin looks valid or if the plugin data was already loaded.
	 */
	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader")
	bool LoadPluginData();
	/**
	 * Mounts the Pak Plugin to the engine which will allow its assets to be used.
	 * The plugin data will be Loaded if it was not.
	 * If successful, the Status of this instance will change to  `Mounted`, unless the plugin was already mounted in which case the Status will not change.
	 * @return Returns true if we were able to Mount the plugin or if the plugin was already mounted.
	 */
	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader")
	bool Mount();

	/**
	 * Activate the GameFeatures of this Pak Plugin. This function is Asynchronous and the CompleteDelegate callback will be called on completion.
	 * This has the same effect as activating the plugin with the GameFeaturesSubsystem, but keeps track of it and will ensure the plugin is unregistered when unmounted.
	 * The plugin will be Mounted if it was not.
	 * If successful, the Status of this instance will change first to  `ActivatingGameFeature` and then to `GameFeatureActivated`.
	 * If failed, the Status will end up reverting back to `Mounted`
	 * If the plugin was already Activated, the Status will not change.
	 */
	void ActivateGameFeature(const FOperationCompleted& CompleteDelegate);
	/**
	 * Deactivate the GameFeatures of this Pak Plugin. This function is Asynchronous and the CompleteDelegate callback will be called on completion.
	 * This has the same effect as deactivating the plugin with the GameFeaturesSubsystem, but keeps track of it and will ensure the plugin is unregistered when unmounted.
	 * If successful, the Status of this instance will change first to  `DeactivatingGameFeature` and then to `Mounted`.
	 * If failed, the Status will end up reverting back to `Mounted`
	 * If the plugin was already Deactivated, the Status will not change.
	 */
	void DeactivateGameFeature(const FOperationCompleted& CompleteDelegate);

	/**
	 * Unmounts the Pak Plugin to the engine. Its assets cannot be used anymore.
	 * If successful, the Status of this instance will change to  `Unmounted`, unless the plugin was already mounted in which case the Status will not change.
	 * The plugin will be Deactivated if it was not, but the deactivation might finish later on as it is asynchronous.
	 * @return Returns true if we were able to Unmount the plugin or if the plugin was already Unmounted.
	 */
	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader")
	bool Unmount();
	/**
	 * Deinitialize the plugin and unloads the its Plugin Data. This is automatically called on Destruction.
	 * If successful, the Status of this instance will change to  `NotInitialized`.
	 * The plugin will be Unmounted and Deactivated if it was not, but the deactivation might finish later on as it is asynchronous.
	 */
	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader")
	void Deinitialize();

	/**
	 * Return the list of the all the assets in the Pak Plugin.
	 * Only Valid if Status is >= `Mounted`
	 * @param Paths The Array that will hold the Assets Data
	 * @param bIncludeCookGeneratedAssets If set to false (default value), will exclude the assets generated by the cooker (having the flag PKG_CookGenerated)
	 * @return Returns true if we were able to retrieve the list of files, meaning the Pak Plugin is Mounted.
	 */
	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader", meta=(AdvancedDisplay=1))
	bool GetAllPluginAssets(TArray<FAssetData>& Paths, bool bIncludeCookGeneratedAssets = false);
	/**
	 * Return the list of the all the UWorld in the Pak Plugin.
	 * Only Valid if Status is >= `Mounted`
	 * @param Class The class of the assets to find
	 * @param Paths The Array that will hold the Assets Data
	 * @param bIncludeCookGeneratedAssets If set to false (default value), will exclude the assets generated by the cooker (having the flag PKG_CookGenerated)
	 * @return Returns true if we were able to retrieve the list of files, meaning the Pak Plugin is Mounted.
	 */
	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader", meta=(AdvancedDisplay=2))
	bool GetPluginAssetsOfClass(const UClass* Class, TArray<FAssetData>& Paths, bool bIncludeCookGeneratedAssets = false);
	/**
	 * Return the list of the all the UWorld in the Pak Plugin.
	 * Only Valid if Status is >= `Mounted`
	 * @param Paths The Array that will hold the Assets Data
	 * @param bIncludeCookGeneratedAssets If set to false (default value), will exclude the assets generated by the cooker (having the flag PKG_CookGenerated)
	 * @return Returns true if we were able to retrieve the list of files, meaning the Pak Plugin is Mounted.
	 */
	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader", meta=(Keywords="Map, Level", AdvancedDisplay=1))
	bool GetPluginWorldAssets(TArray<FAssetData>& Paths, bool bIncludeCookGeneratedAssets = false)
	{
		return GetPluginAssetsOfClass(UWorld::StaticClass(), Paths, bIncludeCookGeneratedAssets);
	}

	/**
	 * Return the array of Assets of the given class cached by the PluginAssetRegistry.
	 * Only Valid if Status is >= `Mounted`
	 * If you want to filter the assets, use GetPluginAssetsOfClass(const UClass*, TArray<FAssetData>&, bool) instead, or directly PluginAssetRegistry->GetAssets
	 */
	const TArray<const FAssetData*>& GetPluginAssetsOfClass(const FTopLevelAssetPath& ClassPathName);
	const TArray<const FAssetData*>& GetPluginAssetsOfClass(const UClass* Class)
	{
		return IsValid(Class) ? GetPluginAssetsOfClass(Class->GetClassPathName()) : EmptyAssetsData;
	}
	/**
	 * Return the array of UWorld Assets cached by the PluginAssetRegistry.
	 * Only Valid if Status is >= `Mounted`
	 * If you want to filter the assets, use GetPluginAssetsOfClass(const UClass*, TArray<FAssetData>&, bool) instead, or directly PluginAssetRegistry->GetAssets
	 */
	const TArray<const FAssetData*>& GetPluginWorldAssets()
	{
		return GetPluginAssetsOfClass(UWorld::StaticClass()->GetClassPathName());
	}

	/** Returns the path to the Pak Plugin Directory this struct points to, for example 'C:/Pak/my-plugin-name/' */
	const FString& GetPakPluginDirectory() const { return PakPluginDirectory; }

	/**
	 * Sets the path to the Pak Plugin directory. If successful, the Plugin will still have to be loaded by calling LoadPluginData.
	 * We expect a Pak Plugin directory to have these files and folders:
	 *	my-plugin-name/
	 *		my-plugin-name.uplugin
	 *		Content/Paks/<platform>/my-plugin-name-and-additional-things.pak
	 * Here, the path to the plugin directory would be the path to 'my-plugin-name', for example 'C:/Pak/my-plugin-name/'
	 * @return Returns true if we were able to change the Directory of the plugin which is only possible when the plugin is not Mounted.
	 */
	bool SetPakPluginDirectory(const FString& InPluginDirectory)
	{
		if (Status != EGFPakLoaderStatus::Mounted)
		{
			Deinitialize();

			PakPluginDirectory = InPluginDirectory;
			FPaths::NormalizeDirectoryName(PakPluginDirectory);

			return true;
		}
		return false;
	}

	/**
	 * Returns the name of the folder of this Pak Plugin.
	 * If GetPakPluginDirectory returns 'C:/Pak/my-plugin-name/', this function would return `my-plugin-name`
	 */
	FString GetPakPluginDirectoryName() const { return FPaths::GetPathLeaf(GetPakPluginDirectory()); }
	
	/** Returns the current status of this Pak Plugin */
	EGFPakLoaderStatus GetStatus() const { return Status; }
	/** Returns true if the current status of this Pak Plugin is at least or equal to the given one */
	UFUNCTION(BlueprintPure, Category="GameFeatures Pak Loader")
	bool IsStatusLessThan(const EGFPakLoaderStatus MinStatus) const { return Status < MinStatus; }
	/** Returns true if the current status of this Pak Plugin is at most or equal to the given one */
	UFUNCTION(BlueprintPure, Category="GameFeatures Pak Loader")
	bool IsStatusLessOrEqualTo(const EGFPakLoaderStatus MinStatus) const { return Status <= MinStatus; }
	/** Returns true if the current status of this Pak Plugin is at most or equal to the given one */
	UFUNCTION(BlueprintPure, Category="GameFeatures Pak Loader")
	bool IsStatusEqualTo(const EGFPakLoaderStatus MinStatus) const { return Status == MinStatus; }
	UFUNCTION(BlueprintPure, Category="GameFeatures Pak Loader")
	bool IsStatusGreaterOrEqualTo(const EGFPakLoaderStatus MinStatus) const { return Status >= MinStatus; }
	/** Returns true if the current status of this Pak Plugin is at least or equal to the given one */
	UFUNCTION(BlueprintPure, Category="GameFeatures Pak Loader")
	bool IsStatusGreaterThan(const EGFPakLoaderStatus MinStatus) const { return Status > MinStatus; }
	
	/**
	 * Returns the name of this Pak Plugin, for example 'my-plugin-name'.
	 * Only Valid if Status is >= `Unmounted`
	 */
	const FString& GetPluginName() const { return PluginName; }
	/**
	 * Returns the name of this Pak Plugin, for example 'my-plugin-name', or the name of the folder of this plugin if it is not yet initialized.
	 */
	FString GetSafePluginName() const { return PluginName.IsEmpty() ? GetPakPluginDirectoryName() : PluginName; }
	/**
	 * Returns the expected Plugin MountPoint of this Pak Plugin, which should be the plugin name surrounded my '/'. Ex: '/my-plugin-name/'
	 * Only Valid if Status is >= `Unmounted`
	 */
	FString GetExpectedPluginMountPoint() const { return Status >= EGFPakLoaderStatus::Unmounted ? TEXT("/") + PluginName + TEXT("/") : TEXT(""); }
	/**
	 * Returns the path to the .pak file of this Pak Plugin, for example 'C:/Pak/my-plugin-name/Content/Paks/Windows/my-plugin-name-and-additional-things.pak'.
	 * Only Valid if Status is >= `Unmounted`
	 */
	const FString& GetPakFilePath() const { return PakFilePath; }
	/**
	 * Returns true if this PakPlugin has a .uplugin. PakPlugin are not required to have a .uplugin if UGFPakLoaderSettings::bRequireUPluginPaks is false
	 * Only Valid if Status is >= `Unmounted`
	 */
	bool HasUPlugin() const { return bHasUPlugin; }
	/**
	 * Returns the path to the .uplugin file of this Pak Plugin, for example 'C:/Pak/my-plugin-name/my-plugin-name.uplugin'.
	 * Only Valid if Status is >= `Unmounted` and HasUPlugin() is true
	 */
	const FString& GetUPluginPath() const { return UPluginPath; }
	/**
	 * Returns the plugin descriptor loaded from the .uplugin of this Pak Plugin.
	 * Only Valid if Status is >= `Unmounted` and HasUPlugin() is true
	 */
	const FPluginDescriptor& GetPluginDescriptor() const { return PluginDescriptor; }
	/**
	 * Returns true if the plugin was a GameFeatures plugin.
	 * Only Valid if Status is >= `Unmounted`
	 */
	bool IsGameFeaturesPlugin() const { return bIsGameFeaturesPlugin; }
	/**
	 * Returns the Mount Points added by this Pak Plugin when it was mounted. The first MountPoint should be the plugin MountPoint.
	 * Only Valid if Status is >= `Mounted`
	 */
	const TArray<TSharedPtr<FPluginMountPoint>>& GetPakPluginMountPoints() const { return PakPluginMountPoints; }
	/**
	 * Returns the IPakFile of this mounted PakPlugin
	 * Only Valid if Status is >= `Mounted`
	 */
	IPakFile* GetPakFile() const { return Status >= EGFPakLoaderStatus::Mounted ? MountedPakFile : nullptr; }
	/**
	 * Returns the PluginAsset Registry
	 * Only Valid if Status is >= `Mounted`
	 */
	const FAssetRegistryState* GetPluginAssetRegistry() const { return Status >= EGFPakLoaderStatus::Mounted ? PluginAssetRegistry.GetPtrOrNull() : nullptr; }
	/**
		 * Returns the PluginAsset Registry
		 * Only Valid if Status is >= `Mounted`
		 */
	const TSharedPtr<IPlugin>& GetPluginInterface() const { return PluginInterface; }
	
	/**
	 * Returns the FAssetData pointing to the UGameFeatureData of this GFPakPlugin.
	 * Only Valid if Status is >= `Mounted` and if the plugin is a GameFeatures plugin.
	 */
	const FAssetData* GetGameFeatureData() const;
	
	/**
	 * Return a map of the possible filenames of files present within this pak. Useful to check if a file exists.
	 * The Key is the possible name to look for, and the value, which can be shared between multiple entries, is a
	 * FGFPakFilenameMap containing possible derivation of the filename.
	 * Only Valid if Status is >= `Mounted`
	 * @return 
	 */
	const TMap<FName, TSharedPtr<const FGFPakFilenameMap>>& GetPakFilenamesMap() const { return PakFilenamesMap;};
protected:
	EGFPakLoaderStatus PreviouslyBroadcastedStatus = EGFPakLoaderStatus::NotInitialized;
	bool BroadcastOnStatusChange(EGFPakLoaderStatus NewStatus);
private:
	bool IsValidPakPluginDirectory(const FString& InPakPluginDirectory);

	inline static const FString PaksFolderFromDirectory = TEXT("Content/Paks");
	inline static const TArray<const FAssetData*> EmptyAssetsData = {};
private:
	/** Returns the path to the Pak Plugin Directory this struct points to, for example 'C:/Pak/my-plugin-name/' */
	UPROPERTY(BlueprintReadOnly, Category="GameFeatures Pak Loader", meta = (AllowPrivateAccess = "true"))
	FString PakPluginDirectory;

	/**
	 * Returns the name of this Pak Plugin, for example 'my-plugin-name'.
	 * Only Valid if Status is >= `Unmounted`
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="GameFeatures Pak Loader", meta = (AllowPrivateAccess = "true"))
	FString PluginName;
	/**
	 * Returns the path to the .uplugin file of this Pak Plugin, for example 'C:/Pak/my-plugin-name/my-plugin-name.uplugin'.
	 * Only Valid if Status is >= `Unmounted`
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="GameFeatures Pak Loader", meta = (AllowPrivateAccess = "true"))
	FString PakFilePath;
	/**
	 * PakPlugin are not required to have a .uplugin if UGFPakLoaderSettings::bRequireUPluginPaks is false, so bHasUPlugin returns true if this PakPlugin has a .uplugin
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="GameFeatures Pak Loader", meta = (AllowPrivateAccess = "true"))
	bool bHasUPlugin = false;
	/**
	 * Returns the path to the .uplugin file of this Pak Plugin, for example 'C:/Pak/my-plugin-name/my-plugin-name.uplugin'.
	 * Only Valid if Status is >= `Unmounted`
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="GameFeatures Pak Loader", meta = (AllowPrivateAccess = "true"))
	FString UPluginPath;
	/**
	 * Returns the plugin descriptor loaded from the .uplugin of this Pak Plugin.
	 * Only Valid if Status is >= `Unmounted`
	 */
	FPluginDescriptor PluginDescriptor;

	/** Returns the current status of this Pak Plugin */
	UPROPERTY(Transient, BlueprintReadOnly, Category="GameFeatures Pak Loader", meta = (AllowPrivateAccess = "true"))
	EGFPakLoaderStatus Status = EGFPakLoaderStatus::NotInitialized;
	/**
	 * Returns true if the plugin was a GameFeatures plugin.
	 * Only Valid if Status is >= `Unmounted`
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="GameFeatures Pak Loader", meta = (AllowPrivateAccess = "true"))
	bool bIsGameFeaturesPlugin = false;

	TArray<TSharedPtr<FPluginMountPoint>> PakPluginMountPoints;
	IPakFile* MountedPakFile = nullptr;

	UPROPERTY(BlueprintAssignable, Category="GameFeatures Pak Loader", DisplayName = "On Deactivating Game Features", meta = (AllowPrivateAccess = "true"))
	FPluginEvent OnDeactivatingGameFeaturesDelegate;
	UPROPERTY(BlueprintAssignable, Category="GameFeatures Pak Loader", DisplayName = "On Unmounting", meta = (AllowPrivateAccess = "true"))
	FPluginEvent OnUnmountingDelegate;
	UPROPERTY(BlueprintAssignable, Category="GameFeatures Pak Loader", DisplayName = "On Deactivating", meta = (AllowPrivateAccess = "true"))
	FPluginEvent OnDeinitializingDelegate;
	
	UPROPERTY(BlueprintAssignable, Category="GameFeatures Pak Loader", DisplayName = "On Status Changed", meta = (AllowPrivateAccess = "true"))
	FOnStatusChanged OnStatusChangedDelegate;
	UPROPERTY(BlueprintAssignable, Category="GameFeatures Pak Loader", DisplayName = "On Begin Destroy", meta = (AllowPrivateAccess = "true"))
	FPluginEvent OnBeginDestroyDelegate;
	UPROPERTY(BlueprintAssignable, Category="GameFeatures Pak Loader", DisplayName = "On Destroyed", meta = (AllowPrivateAccess = "true"))
	FPluginEvent OnDestroyedDelegate;

	TOptional<FAssetRegistryState> PluginAssetRegistry;

	bool bNeedGameFeatureUnloading = false;
	TArray<FOperationCompleted> AdditionalActivationDelegate;
	TArray<FOperationCompleted> AdditionalDeactivationDelegate;

	TSharedPtr<IPlugin> PluginInterface = nullptr;
	
	TMap<FName, TSharedPtr<const FGFPakFilenameMap>> PakFilenamesMap;
private:

	// Internal functions that do all the work but do not broadcast the change of Status
	bool LoadPluginData_Internal();
	bool Mount_Internal();
	void ActivateGameFeature_Internal(const FOperationCompleted& CompleteDelegate);
	void DeactivateGameFeature_Internal(const FOperationCompleted& CompleteDelegate);
	void UnloadPakPluginObjects_Internal(const FOperationCompleted& CompleteDelegate);
	bool Unmount_Internal();
	void Deinitialize_Internal();

	static void PurgePakPluginContent(const FString& PakPluginName, const TFunctionRef<bool(UObject*)>& ShouldObjectBePurged, bool bMarkAsGarbage = true);
	/** Returns true if the object and its children were purged, otherwise false */
	static bool PurgeObject(UObject* Object, const TFunctionRef<bool(UObject*)>& ShouldObjectBePurged, TArray<UObject*>& ObjectsToPurge, TArray<UObject*>& PublicObjectsToPurge, bool bMarkAsGarbage = true);
	
	// Base error message used internally for logging
	FString GetBaseErrorMessage(const FString& Action) const
	{
		return TEXT("Error ") + Action + TEXT(" the Pak Plugin data for directory '") + PakPluginDirectory + TEXT("'");
	}

private: // adjusted functions of FPluginUtils::LoadPlugin, FPluginUtils::UnloadPlugin and UPackageTools::UnloadPackages to run at runtime
	/**
	 * Adjusted version of FPluginUtils::LoadPlugin for GF Pak Plugin, also working at runtime
	 * @return IPlugin if successful, otherwise null
	 */
	static TSharedPtr<IPlugin> LoadPlugin(const FString& UPluginFileName, FText* OutFailReason = nullptr);
	/**
	 * Adjusted version of FPluginUtils::UnloadPlugin for GF Pak Plugin, also working at runtime
	 */
	static bool UnloadPlugin(const TSharedRef<IPlugin>& Plugin, FText* OutFailReason = nullptr);
	/**
	 * Adjusted version of FPluginUtils::UnloadPluginsAssets for GF Pak Plugin, also working at runtime
	 * Unload assets from the specified plugins but does not unmount them
	 * @warning Dirty assets that need to be saved will be unloaded anyway
	 * @param Plugins Plugins to unload assets from
	 * @param OutFailReason Outputs the reason of the failure if any
	 * @return Whether plugin assets were successfully unloaded
	 */
	static bool UnloadPluginsAssets(const TConstArrayView<TSharedRef<IPlugin>> Plugins, FText* OutFailReason = nullptr);
	/**
	 * Adjusted version of FPluginUtils::UnloadPluginsAssets for GF Pak Plugin, also working at runtime
	 * Unload assets from the specified plugin but does not unmount them
	 * @warning Dirty assets that need to be saved will be unloaded anyway
	 * @param PluginNames Names of the plugins to unload assets from
	 * @param OutFailReason Outputs the reason of the failure if any
	 * @return Whether plugin assets were successfully unloaded
	 */
	static bool UnloadPluginsAssets(const TSet<FString>& PluginNames, FText* OutFailReason = nullptr);
	/**
	 * Adjusted version of UPackageTools::UnloadPackages for GF Pak Plugin, also working at runtime
	 * Helper function that attempts to unload the specified top-level packages.
	 *
	 * @param	PackagesToUnload		the list of packages that should be unloaded
	 * @param	OutErrorMessage			An error message specifying any problems with unloading packages
	 * @param	bUnloadDirtyPackages	Whether to unload packages that are dirty (that need to be saved)
	 *
	 * @return	true if the set of loaded packages was changed
	 */
	static bool UnloadPackages(const TArray<UPackage*>& PackagesToUnload, FText& OutErrorMessage, bool bUnloadDirtyPackages = false);

#if WITH_EDITOR
	static void RestoreStandaloneOnReachableObjects();

	static TSet<UPackage*>* PackagesBeingUnloaded;
	static TSet<UObject*> ObjectsThatHadFlagsCleared;
	static FDelegateHandle ReachabilityCallbackHandle;
#endif

private:
	enum class EAddOrRemove : uint8
	{
		Add,
		Remove,
	};
	/**
	 * [Workaround] Remove the given PackageNames (ex: "/Game/Folder/BP_Actor") to the Asset Registry CachedEmptyPackages.
	 * This is needed as AssetRegistry::AppendState does not take care of this and assets will not be removed from this cache once reloaded
	 */
	static void AddOrRemovePackagesFromAssetRegistryEmptyPackagesCache(EAddOrRemove Action, const TSet<FName>& PackageNames);

	void UnregisterPluginAssetsFromAssetRegistry();

#if WITH_EDITOR
public:
	const TPair<FString, FString>& GetMountPointAboutToBeMounted() const { return MountPointAboutToBeMounted; }
private:
	// Set just before creating a MountPoint for the ContentBrowser to refer to. Ex: {"/DLCTestProject/", "/../../../DLCTestProject/Content/"}
	TPair<FString, FString> MountPointAboutToBeMounted;
#endif
};