// Copyright GeoTech BV


#include "GFPakPlugin.h"

#include "ComponentRecreateRenderStateContext.h"
#include "ComponentReregisterContext.h"
#include "GameFeatureData.h"
#include "GameFeaturePluginOperationResult.h"
#include "GameFeaturesSubsystem.h"
#include "GameMapsSettings.h"
#include "GFPakLoaderLog.h"
#include "GFPakLoaderPlatformFile.h"
#include "GFPakLoaderSettings.h"
#include "GFPakLoaderSubsystem.h"
#include "IPlatformFilePak.h"
#include "PluginDescriptor.h"
#include "PluginMountPoint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "Engine/AssetManager.h"
#include "Engine/Level.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManagerGeneric.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"
#include "Misc/PathViews.h"
#include "UObject/LinkerLoad.h"

#if WITH_EDITOR
#include "ObjectTools.h"
#include "PackageTools.h"
#include "Selection.h"
#include "Editor/Transactor.h"
#include "Elements/Interfaces/TypedElementSelectionInterface.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

#define LOCTEXT_NAMESPACE "FGFPakLoaderModule"


class FPakFileLister : public IPlatformFile::FDirectoryVisitor
{
public:
	/**
	 * 
	 * @param InMountPath The Mount Path returned by the IPakFile which will be prefixed to each path retrieved
	 * @param bInReturnMountedPaths If true, will return the Mounted Paths (ex: "/Engine/EngineMaterials/FlatNormal") instead of the raw paths (ex: "../../../Engine/Content/EngineMaterials/FlatNormal")
	 * @param bInIncludeExtension If true, the paths will contain extensions (like .uasset, .umap)
	 * @param bInIncludeNonAssets If true, all files will be returned, even the .ubulk and .uexp
	 * @param InTargetExtension If not empty, will only return files matching the given extension (need to include the ".") ex: ".umap"
	 */
	FPakFileLister(const FString& InMountPath, bool bInReturnMountedPaths, bool bInIncludeExtension, bool bInIncludeNonAssets, const FString& InTargetExtension = FString())
		 : MountPath(InMountPath)
		, bReturnMountedPaths(bInReturnMountedPaths)
		, bIncludeExtension(bInIncludeExtension)
		, bIncludeNonAssets(bInIncludeNonAssets)
		, TargetExtension(InTargetExtension)
	{}
	
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		if (!bIsDirectory)
		{
			const FString Fullname = MountPath + FilenameOrDirectory;
			if (bReturnMountedPaths)
			{
				FString LocalPath; // ex: "../../../Engine/Content/EngineMaterials/FlatNormal"
				FString PackageName; //ex: "/Engine/EngineMaterials/FlatNormal"
				FString Extension; //ex: ".uasset" or ".umap"
				if (FPackageName::TryConvertToMountedPath(Fullname, &LocalPath, &PackageName, nullptr, nullptr, &Extension))
				{
					if (bIncludeNonAssets || Extension == TEXT(".uasset") || Extension == TEXT(".umap"))
					{
						if (TargetExtension.IsEmpty() || Extension == TargetExtension)
						{
							Paths.Add(bIncludeExtension ? PackageName + Extension : PackageName);
						}
					}
				}
			}
			else
			{
				const FString Extension = FPaths::GetExtension(Fullname, true);
				if (bIncludeNonAssets || Extension == TEXT(".uasset") || Extension == TEXT(".umap"))
				{
					if (TargetExtension.IsEmpty() || Extension == TargetExtension)
					{
						Paths.Add(bIncludeExtension ? Fullname : FPaths::GetBaseFilename(Fullname, false));
					}
				}
			}
		}
		return true;
	}	
	FString MountPath;
	bool bReturnMountedPaths;
	bool bIncludeExtension;
	bool bIncludeNonAssets;
	FString TargetExtension;
	TArray<FString> Paths;
};


class FPakFilenameFinder : public IPlatformFile::FDirectoryVisitor
{
public:
	FPakFilenameFinder(const FString& InFilename) : Filename(InFilename) {}
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		if (!bIsDirectory && Filename == FPaths::GetCleanFilename(FilenameOrDirectory))
		{
			Result = FilenameOrDirectory;
			return false;
		}
		return true;
	}
	FString Filename;
	FString Result;
};


class FPakContentFoldersFinder : public IPlatformFile::FDirectoryVisitor
{
public:
	FPakContentFoldersFinder(const FString& InMountPath) : MountPath(InMountPath) {}
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		if (!bIsDirectory)
		{
			const FString Fullname = MountPath + FilenameOrDirectory;
			FString PreContent;
			if (Fullname.Split(TEXT("/Content/"), &PreContent, nullptr))
			{
				ContentFolders.AddUnique(PreContent + TEXT("/Content/"));
			}
		}
		return true;
	}
	FString MountPath;
	TArray<FString> ContentFolders;
};



void UGFPakPlugin::BeginDestroy()
{
	// We want to be sure all the delegates have been fired
	for (FOperationCompleted& Delegate : AdditionalActivationDelegate)
	{
		Delegate.ExecuteIfBound(false, {});
	}
	AdditionalActivationDelegate.Empty();
	for (FOperationCompleted& Delegate : AdditionalDeactivationDelegate)
	{
		Delegate.ExecuteIfBound(false, {});
	}
	AdditionalDeactivationDelegate.Empty();
	
	Deinitialize_Internal();
	UObject::BeginDestroy();
}

bool UGFPakPlugin::LoadPluginData()
{
	const bool Result = LoadPluginData_Internal();
	BroadcastOnStatusChange(Status);
	return Result;
}

bool UGFPakPlugin::Mount()
{
	const bool Result = Mount_Internal();
	BroadcastOnStatusChange(Status);
	if (bIsGameFeaturesPlugin && Status == EGFPakLoaderStatus::Mounted && GetDefault<UGFPakLoaderSettings>()->bAutoMountPakPlugins)
	{
		const FAssetData* GFDataAsset = UGFPakPlugin::GetGameFeatureData();
		UGameFeatureData* GFData = Cast<UGameFeatureData>(GFDataAsset->GetAsset());
		if (ensure(GFData))
		{
			TSharedPtr<FJsonObject> PluginDescriptorJsonObject;
#if WITH_EDITOR
			PluginDescriptorJsonObject = PluginDescriptor.CachedJson;
#else
			FText OutFailReason;
			FString JsonText;
			if (FFileHelper::LoadFileToString(JsonText, *UPluginPath))
			{
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
				if (!FJsonSerializer::Deserialize(Reader, PluginDescriptorJsonObject) || !PluginDescriptorJsonObject.IsValid())
				{
					PluginDescriptorJsonObject.Reset();
				}
			}
#endif
			if (PluginDescriptorJsonObject)
			{
				EBuiltInAutoState InitialState = UGameFeaturesSubsystem::DetermineBuiltInInitialFeatureState(PluginDescriptorJsonObject, this->PluginName);
				if (InitialState == EBuiltInAutoState::Active)
				{
					const auto EmptyLambda = FOperationCompleted::CreateLambda([](const bool bSuccessful, const TOptional<UE::GameFeatures::FResult>& Result){});
					ActivateGameFeature(EmptyLambda);
				}
			}
		}
	}
	return Result;
}

void UGFPakPlugin::ActivateGameFeature(const FOperationCompleted& CompleteDelegate)
{
	ActivateGameFeature_Internal(FOperationCompleted::CreateLambda(
		[WeakThis = TWeakObjectPtr<UGFPakPlugin>(this), CompleteDelegate = CompleteDelegate](const bool bSuccessful, const TOptional<UE::GameFeatures::FResult>& Result)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->BroadcastOnStatusChange(WeakThis->Status);
			}
			CompleteDelegate.ExecuteIfBound(bSuccessful, Result);
		}));
}

void UGFPakPlugin::DeactivateGameFeature(const FOperationCompleted& CompleteDelegate)
{
	DeactivateGameFeature_Internal(FOperationCompleted::CreateLambda(
		[WeakThis = TWeakObjectPtr<UGFPakPlugin>(this), CompleteDelegate = CompleteDelegate](const bool bSuccessful, const TOptional<UE::GameFeatures::FResult>& Result)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->BroadcastOnStatusChange(WeakThis->Status);
			}
			CompleteDelegate.ExecuteIfBound(bSuccessful, Result);
		}));
}

bool UGFPakPlugin::Unmount()
{
	const bool Result = Unmount_Internal();
	BroadcastOnStatusChange(Status);
	return Result;
}

void UGFPakPlugin::Deinitialize()
{
	Deinitialize_Internal();
	BroadcastOnStatusChange(Status);
}


bool UGFPakPlugin::GetAllPluginAssets(TArray<FAssetData>& Paths, bool bIncludeCookGeneratedAssets)
{
	if (Status >= EGFPakLoaderStatus::Mounted && MountedPakFile != nullptr)
	{
		if (!bIncludeCookGeneratedAssets)
		{
			FARCompiledFilter Filter;
			Filter.WithoutPackageFlags = PKG_CookGenerated;
			return PluginAssetRegistry->GetAssets(Filter,{},Paths );
		}
		else
		{
			return PluginAssetRegistry->GetAllAssets({},Paths );
		}
	}
	UE_LOG(LogGFPakLoader, Warning, TEXT("Tried to retrieve the list of files from the non-Mounted Pak Plugin '%s'"), *PakPluginDirectory)
	Paths.Empty();
	return false;
}

bool UGFPakPlugin::GetPluginAssetsOfClass(const UClass* Class, TArray<FAssetData>& Paths, bool bIncludeCookGeneratedAssets)
{
	if (IsValid(Class) && Status >= EGFPakLoaderStatus::Mounted && ensure(PluginAssetRegistry.IsSet()))
	{
		FARCompiledFilter Filter;
		Filter.ClassPaths.Add(Class->GetClassPathName());
		if (!bIncludeCookGeneratedAssets)
		{
			Filter.WithoutPackageFlags = PKG_CookGenerated;
		}
		return PluginAssetRegistry->GetAssets(Filter,{},Paths );
	}
	UE_LOG(LogGFPakLoader, Warning, TEXT("Tried to retrieve the list of files from the non-Mounted Pak Plugin '%s'"), *PakPluginDirectory)
	Paths.Empty();
	return false;
}

const TArray<const FAssetData*>& UGFPakPlugin::GetPluginAssetsOfClass(const FTopLevelAssetPath& ClassPathName)
{
	if (Status >= EGFPakLoaderStatus::Mounted && ensure(PluginAssetRegistry.IsSet()))
	{
		return PluginAssetRegistry->GetAssetsByClassPathName(ClassPathName);
	}
	return EmptyAssetsData;
}

const FAssetData* UGFPakPlugin::GetGameFeatureData() const
{
	if (!PluginAssetRegistry)
	{
		return nullptr;
	}
	
	TArray<const FAssetData*> AllGameFeaturesData = PluginAssetRegistry->GetAssetsByClassPathName(UGameFeatureData::StaticClass()->GetClassPathName());
	const FAssetData** GameFeaturesData = Algo::FindByPredicate(AllGameFeaturesData, [](const FAssetData* AssetData)
	{
		if (ensure(AssetData))
		{
			const FString PackagePath = AssetData->PackagePath.ToString().Replace(TEXT("/"), TEXT(""));;
			const FString AssetName = AssetData->AssetName.ToString();
			return PackagePath == AssetName;
		}
		return false;
	});
	return GameFeaturesData ? *GameFeaturesData : nullptr;
}

UGFPakLoaderSubsystem* UGFPakPlugin::GetSubsystem() const
{
	return Cast<UGFPakLoaderSubsystem>(GetOuter());
}

bool UGFPakPlugin::BroadcastOnStatusChange(EGFPakLoaderStatus NewStatus)
{
	if (NewStatus != PreviouslyBroadcastedStatus)
	{
		Status = NewStatus;
		const EGFPakLoaderStatus OldStatus = PreviouslyBroadcastedStatus; 
		PreviouslyBroadcastedStatus = NewStatus; // we don't know what might happen when we broadcast the event, so we need to update this value first
		UE_LOG(LogGFPakLoader, Verbose, TEXT("Pak Plugin '%s' broadcasting status change:  '%s' => '%s'"), *PakPluginDirectory, *UEnum::GetValueAsString(OldStatus), *UEnum::GetValueAsString(NewStatus))
		OnStatusChangedDelegate.Broadcast(this, OldStatus, NewStatus);
		return true;
	}
	return false;
}

bool UGFPakPlugin::IsValidPluginDirectory(const FString& InPluginDirectory)
{
	const FString BaseErrorMessage = GetBaseErrorMessage(TEXT("Validating"));
	
	// First, we ensure that the plugin directory exist.
	if (!FPaths::DirectoryExists(PakPluginDirectory))
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("%s: Directory does not exist"), *BaseErrorMessage)
		return false;
	}

	// Then we ensure that we have the plugin data we need.
	PluginName = FPaths::GetBaseFilename(PakPluginDirectory);
	UPluginPath = PakPluginDirectory / PluginName + TEXT(".uplugin");
	if (!FPaths::FileExists(UPluginPath))
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("%s: UPlugin file does not exist at location '%s'"), *BaseErrorMessage, *UPluginPath)
		return false;
	}

	// ensure we have the right base directory
	const FString PluginPaksFolder = PakPluginDirectory / PaksFolderFromDirectory;
	if (!FPaths::DirectoryExists(PluginPaksFolder))
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("%s: Directory does not exist"), *BaseErrorMessage)
		return false;
	}

	// then look for the Pak file to load
	IFileManager& FileManager = IFileManager::Get();
	TArray<FString> PakFiles;
	FileManager.FindFilesRecursive(PakFiles, *PluginPaksFolder, TEXT("*.pak"), true, false, false);
	if (PakFiles.IsEmpty())
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("%s: Pak file not found"), *BaseErrorMessage)
		return false;
	}
	else if(PakFiles.Num() > 1)
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("%s: Plugins with only one pak file are supported at the moment"), *BaseErrorMessage)
		return false;
	}

	// and ensure the plugin was not packaged with IO Store for now. todo: handle IO Store
	PakFilePath = PakFiles[0];
	const FString UtocPath = FPaths::ChangeExtension(PakFilePath, TEXT(".utoc"));
	if (FPaths::FileExists(UtocPath))
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("%s: Pak Loader currently does not support packaged plugin with IO Store. Make sure IO Store is turned off in the Project Settings > Packaging."), *BaseErrorMessage)
		return false;
	}
	return true;
}


bool UGFPakPlugin::LoadPluginData_Internal()
{
	const FString BaseErrorMessage = GetBaseErrorMessage(TEXT("Loading"));

	if (Status != EGFPakLoaderStatus::NotInitialized && Status != EGFPakLoaderStatus::InvalidPluginDirectory)
	{
		UE_LOG(LogGFPakLoader, Warning, TEXT("%s: Trying to load the Plugin data of a Pak Plugin that is already loaded."), *BaseErrorMessage)
		return true;
	}

	// 1. We make sure we have the Subsystem as outer, which makes it easy to be retrieved
	const UGFPakLoaderSubsystem* PakLoaderSubsystem = GetSubsystem();
	if (!ensure(IsValid(PakLoaderSubsystem)))
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("%s: The UGFPakPlugin needs to have its UGFPakLoaderSubsystem as Outer."), *BaseErrorMessage)
		Deinitialize_Internal();
		BroadcastOnStatusChange(EGFPakLoaderStatus::NotInitialized);
		return false;
	}

	// 2. We ensure the given plugin directory is a valid one and we set our variables
	if (!IsValidPluginDirectory(PakPluginDirectory))
	{
		Deinitialize_Internal();
		BroadcastOnStatusChange(EGFPakLoaderStatus::InvalidPluginDirectory);
		return false;
	}

	PluginDescriptor = {};
	PluginDescriptor.Load(UPluginPath);

	UE_LOG(LogGFPakLoader, Log, TEXT("Loaded the Pak Plugin data for '%s'"), *PakPluginDirectory)
	UE_LOG(LogGFPakLoader, Log, TEXT("  PluginName : '%s'"), *PluginName)
	UE_LOG(LogGFPakLoader, Log, TEXT("  UPluginPath: '%s'"), *UPluginPath)
	UE_LOG(LogGFPakLoader, Log, TEXT("  UPlugin    : Name: '%s', Version: '%s', Description: '%s'"), *PluginDescriptor.FriendlyName, *PluginDescriptor.VersionName, *PluginDescriptor.Description)
	UE_LOG(LogGFPakLoader, Log, TEXT("  PluginPak  : '%s'"), *PakFilePath)

	BroadcastOnStatusChange(EGFPakLoaderStatus::Unmounted);
	return true;
}

bool UGFPakPlugin::Mount_Internal()
{
	const FString BaseErrorMessage = GetBaseErrorMessage(TEXT("Mounting"));
	// 1. we ensure that we have loaded the plugin data...
	if (Status == EGFPakLoaderStatus::NotInitialized)
	{
		UE_LOG(LogGFPakLoader, Warning, TEXT("%s: Trying to mount a Pak Plugin that is not in an Unmounted state."), *BaseErrorMessage)
		if (!LoadPluginData_Internal())
		{
			return false;
		}
	}
	//... and that we are Unmounted
	if (Status != EGFPakLoaderStatus::Unmounted)
	{
		UE_LOG(LogGFPakLoader, Warning, TEXT("%s: Trying to mount a Pak Plugin that is not in an Unmounted state."), *BaseErrorMessage)
		return Status >= EGFPakLoaderStatus::Mounted;
	}

	UE_LOG(LogGFPakLoader, Log, TEXT("Mounting the Pak Plugin '%s'..."), *PakFilePath)
	
	// 2. We ensure we can actually Mount the Pak file by retrieving the PakPlatformFile and checking if the MountPak delegate is bound
	FGFPakLoaderPlatformFile* PakPlatformFile = GetSubsystem() ? GetSubsystem()->GetGFPakPlatformFile() : nullptr; // We need to ensure the PakPlatformFile is loaded or the following might not work
	if (!ensure(PakPlatformFile) || !ensure(PakPlatformFile->GetPakPlatformFile()) || !FCoreDelegates::MountPak.IsBound())
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("  %s: The Mounting Delegate is not bound. Not able to Mount the Pak file."), *BaseErrorMessage)
		return false;
	}
	
	// 3. Then we do the actual mounting
	MountedPakFile = FCoreDelegates::MountPak.Execute(PakFilePath, INDEX_NONE); // ends up calling FPakPlatformFile::HandleMountPakDelegate and FPakPlatformFile::Mount
	if (!MountedPakFile)
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("  %s: Unable to mount the Pak Plugin '%s'"), *BaseErrorMessage, *PakFilePath)
		Unmount_Internal();
		return false;
	}
	UE_LOG(LogGFPakLoader, Log, TEXT("  Mounted the Pak Plugin '%s'"), *PakFilePath)
	
	// 4a. Now we need to register the proper MountPoint. The default MountPoint returned by the PakFile is the base folder of all the content that was packaged.
	// Depending on what was packaged in the plugin (like Engine content), the default mount point could be something like "../../../" or "../../../<project-name>/"
	// but we need the mount point to be "../../../<project-name>/Plugins[/GameFeatures]/<plugin-name>/Content/"
	// To find the proper MountPoint, we look for the AssetRegistry.bin which is located at the root of the plugin folder
	FString MountPoint = MountedPakFile->PakGetMountPoint();
	UE_LOG(LogGFPakLoader, Log, TEXT("  Default Mount Point '%s'"), *MountPoint)
	// The call to FPaths::MakeStandardFilename does not return the same value on Editor and Game Builds:
	// Example below with FPaths::MakeStandardFilename("../../../StatcastUnreal/Plugins/GameFeatures/hou-minute-maid-park/AssetRegistry.bin")
	// This is because FPlatformProcess::BaseDir() doesn't return the same value:
	// - On Editor Build, it returns the path to the Editor: "C:/Program Files/Epic Games/UE_5.3/Engine/Binaries/Win64",
	//   which is relative to RootDir "C:/Program Files/Epic Games/UE_5.3/", so MakeStandardFilename keeps the given path RELATIVE
	// - On Game Build, it returns the path to the Game Exe: "D:/.../ARL/statcastunreal/Binaries/Win64/", which is not relative to RootDir, so FullPath becomes ABSOLUTE
	// For the files to be found properly, it needs to start with the MountPoint, so instead we adjust the Mount Point and adjust the filename so it does not get adjusted
	static_cast<FPakFile*>(MountedPakFile)->SetMountPoint(*(TEXT("/") + MountPoint)); // adding a starting "/" stops FPaths::MakeStandardFilename from changing the path
	MountPoint = MountedPakFile->PakGetMountPoint();
	UE_LOG(LogGFPakLoader, Log, TEXT("  Adjusted Mount Point '%s'"), *MountPoint) // to it doesn't get expanded
	
	FString AssetRegistryPath;
	{
		// First we look for the AssetRegistry.bin path
		FPakFilenameFinder FileFinder{ TEXT("AssetRegistry.bin") };
		MountedPakFile->PakVisitPrunedFilenames(FileFinder);
		if (FileFinder.Result.IsEmpty())
		{
			UE_LOG(LogGFPakLoader, Error, TEXT("  %s: Unable to find the 'AssetRegistry.bin' content file."), *BaseErrorMessage)
			Unmount_Internal();
			return false;
		}
		AssetRegistryPath = MountPoint + FileFinder.Result;
		UE_LOG(LogGFPakLoader, Log, TEXT("  AssetRegistryPath: '%s'"), *AssetRegistryPath)
		// at this point, AssetRegistryPath should be equal to "/../../../<project-name>/Plugins[/GameFeatures]/<plugin-name>/AssetRegistry.bin"
	}
	FString AssetRegistryFolder = FPaths::GetPath(AssetRegistryPath);
	bIsGameFeaturesPlugin = false;
	{ // Then we find the project name and check if this was a GameFeatures plugin
		if (!AssetRegistryFolder.EndsWith(PluginName))
		{
			UE_LOG(LogGFPakLoader, Error, TEXT("  %s: The path of the 'AssetRegistry.bin' is not from the right plugin: '%s'."), *BaseErrorMessage, *AssetRegistryPath)
			Unmount_Internal();
			return false;
		}
		FString PathToProject, PathAfterPlugins;
		if (!AssetRegistryFolder.Split(TEXT("/Plugins/"), &PathToProject, &PathAfterPlugins))
		{
			UE_LOG(LogGFPakLoader, Error, TEXT("  %s: The path of the 'AssetRegistry.bin' is not in a Plugins folder: '%s'."), *BaseErrorMessage, *AssetRegistryPath)
			Unmount_Internal();
			return false;
		}
		bIsGameFeaturesPlugin = PathAfterPlugins.StartsWith(TEXT("GameFeatures/"));
		// Even though the path makes us believe that this plugin is a GameFeaturesPlugin, it might not have the required GameFeatureData, which we check below once we have access to the Asset Registry.
	}
	UE_LOG(LogGFPakLoader, Log, TEXT("  bIsGameFeaturePlugin: '%s'"), bIsGameFeaturesPlugin ? TEXT("TRUE") : TEXT("FALSE"))

	// 4b. Now that we know the path of the plugin folder, we can add the main Plugin mount point
	UGFPakLoaderSubsystem* PakLoaderSubsystem = GetSubsystem();
	if (ensure(PakLoaderSubsystem))
	{ // We add the plugin Mount Point
		const FString PluginMountPointPath = AssetRegistryFolder / TEXT("Content/");
		UE_LOG(LogGFPakLoader, Log, TEXT("  Adding Mount Point for Pak Plugin:  '%s' => '%s'"), *GetExpectedPluginMountPoint(), *PluginMountPointPath)
		if (TSharedPtr<FPluginMountPoint> ContentMountPoint = PakLoaderSubsystem->AddOrCreateMountPointFromContentPath(PluginMountPointPath))
		{
			PakPluginMountPoints.Add(MoveTemp(ContentMountPoint));
		}
		else
		{
			UE_LOG(LogGFPakLoader, Error, TEXT("     => Unable to create the Pak Plugin Mount Point for the content folder: '%s'."), *PluginMountPointPath)
		}
	}
	// 4c. The assets might have been referencing content outside of their own plugin, which should have been packaged in the Pak file too. We need to create a mount point for them
	{ // Then we look at other possible MountPoints
		FPakContentFoldersFinder ContentFoldersFinder {MountPoint};
		MountedPakFile->PakVisitPrunedFilenames(ContentFoldersFinder);
		if (ContentFoldersFinder.ContentFolders.IsEmpty() || !ensure(PakPluginMountPoints.Num() > 0 && PakPluginMountPoints[0].IsValid()))
		{
			UE_LOG(LogGFPakLoader, Error, TEXT("  %s: Unable to find any Content folder."), *BaseErrorMessage)
			Unmount_Internal();
			return false;
		}
		UE_LOG(LogGFPakLoader, Log, TEXT("  Listing all Pak Plugin Content folders:"))
		for (const FString& ContentFolder : ContentFoldersFinder.ContentFolders)
		{
			UE_LOG(LogGFPakLoader, Log, TEXT("   - '%s'"), *ContentFolder)
			if (ContentFolder != PakPluginMountPoints[0]->GetContentPath() && ensure(PakLoaderSubsystem))
			{
				if (TSharedPtr<FPluginMountPoint> ContentMountPoint = PakLoaderSubsystem->AddOrCreateMountPointFromContentPath(ContentFolder))
				{
					PakPluginMountPoints.Add(MoveTemp(ContentMountPoint));
				}
				else
				{
					UE_LOG(LogGFPakLoader, Error, TEXT("     => Unable to create the Mount Point for the content folder: '%s'."), *ContentFolder)
				}
			}
			else
			{
				UE_LOG(LogGFPakLoader, Log, TEXT("     => Pak Plugin Mount Point (already registered):  '%s' => '%s'"), *PakPluginMountPoints[0]->GetRootPath(), *PakPluginMountPoints[0]->GetContentPath())
			}
		}
	}
	
	// 4d. As we have the asset registry, we can start loading the assets inside the Asset Registry.
	if (IFileHandle* FileHandle = PakPlatformFile->OpenRead(*AssetRegistryPath))
	{
		FArchiveFileReaderGeneric FileReader {FileHandle, *AssetRegistryPath, FileHandle->Size()};
		
		FAssetRegistryVersion::Type Version;
		FAssetRegistryLoadOptions Options(UE::AssetRegistry::ESerializationTarget::ForDevelopment);
		PluginAssetRegistry = FAssetRegistryState{};
		PluginAssetRegistry->Load(FileReader, Options, &Version);
		UE_LOG(LogGFPakLoader, Log, TEXT("  AssetRegistry Loaded from '%s': %d Assets in %d Packages"), *AssetRegistryPath, PluginAssetRegistry->GetNumAssets(), PluginAssetRegistry->GetNumPackages());

		IAssetRegistry& AssetRegistry = UAssetManager::Get().GetAssetRegistry();
#if WITH_EDITOR
		UWorld* World = GetWorld();
		UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
		// In PIE, we don't want the Editor Engine to load the DLC Maps which it will do by default when new World Assets are loaded
		// to avoid this, we remove its delegate before adding the assets to the registry, then we revert the delegate list
		if (World && World->IsPlayInEditor() && EditorEngine)
		{
			TMulticastDelegate<void(UObject*)> OnAssetLoadedDelegates = FCoreUObjectDelegates::OnAssetLoaded; // Make a copy of all delegates
			FCoreUObjectDelegates::OnAssetLoaded.RemoveAll(EditorEngine); // Remove the EditorEngine one, this would call UEditorEngine::OnAssetLoaded
			
			AssetRegistry.AppendState(*PluginAssetRegistry); // Add the assets, this will trigger FCoreUObjectDelegates::OnAssetLoaded
			
			FCoreUObjectDelegates::OnAssetLoaded = OnAssetLoadedDelegates; // Revert
		}
		else
#endif
		{
			AssetRegistry.AppendState(*PluginAssetRegistry);
		}
	}
	else
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("  %s: Unable to load the Pak Plugin Asset Registry: '%s'."), *BaseErrorMessage, *AssetRegistryPath)
		Unmount_Internal();
		return false;
	}
	// 4e. Ensure we have a valid GameFeaturesPlugin if we believe it should be one
	if (bIsGameFeaturesPlugin && ensure(PluginAssetRegistry))
	{
		const FAssetData* GameFeaturesData = GetGameFeatureData();
		if (!GameFeaturesData)
		{
			UE_LOG(LogGFPakLoader, Warning, TEXT("  %s: The Pak Plugin is a GameFeatures plugin but was not packaged is a UGameFeatureData asset at the root of its Content directory. The GameFeatures specific actions might not work."), *BaseErrorMessage)
			UE_LOG(LogGFPakLoader, Warning, TEXT("  bIsGameFeaturePlugin: '%s'"), bIsGameFeaturesPlugin ? TEXT("TRUE") : TEXT("FALSE"))
			bIsGameFeaturesPlugin = false;
		}
	}

	// 5. Register the plugin with the Plugin Manager
	{
		FText FailReason;
		{
			// For UGFPakLoaderSubsystem::RegisterMountPoint to not register the wrong mount point in FPluginManager::MountPluginFromExternalSource, we need to have the plugin status to Mounted
			TOptionalGuardValue<EGFPakLoaderPreviousStatus> TemporaryStatus(Status, EGFPakLoaderStatus::Mounted);
			PluginInterface = LoadPlugin(UPluginPath);
		}
		if (PluginInterface)
		{
			UE_LOG(LogGFPakLoader, Log, TEXT("  Successfully loaded plugin from UPlugin '%s'!"), *UPluginPath)
		}
		else
		{
			UE_LOG(LogGFPakLoader, Error, TEXT("  %s: Unable to add the UPlugin '%s' to the plugins list:  '%s'"), *BaseErrorMessage, *UPluginPath, *FailReason.ToString())
			Unmount_Internal();
			return false;
		}
	}
	
	BroadcastOnStatusChange(EGFPakLoaderStatus::Mounted);

	return true;
}

void UGFPakPlugin::ActivateGameFeature_Internal(const FOperationCompleted& CompleteDelegate)
{
	const FString BaseErrorMessage = GetBaseErrorMessage(TEXT("Activating"));
	
	if (Status < EGFPakLoaderStatus::Mounted)
	{
		UE_LOG(LogGFPakLoader, Warning, TEXT("%s: Trying to Activate the GameFeatures of a Pak Plugin that is not in a Mounted state."), *BaseErrorMessage)
		if (!Mount_Internal())
		{
			CompleteDelegate.ExecuteIfBound(false, {});
			return;
		}
	}
	
	if (Status == EGFPakLoaderStatus::ActivatingGameFeature)
	{
		AdditionalActivationDelegate.Add(CompleteDelegate); // We add the delegate to be called when the Activation is done
		return;
	}
	else if (Status == EGFPakLoaderStatus::GameFeatureActivated)
	{
		UE_LOG(LogGFPakLoader, Warning, TEXT("%s: Trying to Activate the GameFeatures of a Pak Plugin that is already Activated."), *BaseErrorMessage)
		CompleteDelegate.ExecuteIfBound(true, {});
		return;
	}
	else if (Status != EGFPakLoaderStatus::Mounted)
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("%s: Trying to Activate the GameFeatures of a Pak Plugin that is not in a Mounted state."), *BaseErrorMessage)
		CompleteDelegate.ExecuteIfBound(false, {});
		return;
	}
	
	
	// Now Register with the GameFeaturesSubsystem if needed
	if (bIsGameFeaturesPlugin && GEngine)
	{
		UE_LOG(LogGFPakLoader, Log, TEXT("Activating GameFeatures for the Pak Plugin '%s'..."), *PakFilePath)
		if(UGameFeaturesSubsystem* GFSubsystem = GEngine->GetEngineSubsystem<UGameFeaturesSubsystem>())
		{
			BroadcastOnStatusChange(EGFPakLoaderStatus::ActivatingGameFeature);
			
			const FString GFPluginPath = UGameFeaturesSubsystem::GetPluginURL_FileProtocol(UPluginPath);
			
			UE_LOG(LogGFPakLoader, Verbose, TEXT("  Calling UGameFeaturesSubsystem::LoadAndActivateGameFeaturePlugin for Pak Plugin '%s'..."), *PluginName)
			GFSubsystem->LoadAndActivateGameFeaturePlugin(GFPluginPath, FGameFeaturePluginLoadComplete::CreateLambda(
				[WeakThis = TWeakObjectPtr<UGFPakPlugin>(this), GFPluginPath, CompleteDelegate](const UE::GameFeatures::FResult& Result)
				{
					UE_CLOG(Result.HasError(), LogGFPakLoader, Error, TEXT("  ... Error while Loading and Activating Game Feature for GF Plugin '%s':  %s"), *GFPluginPath, *Result.GetError())
					UE_CLOG(!Result.HasError(), LogGFPakLoader, Log, TEXT("  ... Finished Loading and Activating Game Feature for GF Plugin '%s'"), *GFPluginPath)

					if (WeakThis.IsValid())
					{
						if (Result.HasError())
						{
							WeakThis->Status = EGFPakLoaderStatus::GameFeatureActivated; //needed even for errors to be sure we are able to deactivate, but no need to broadcast
							WeakThis->DeactivateGameFeature_Internal(FOperationCompleted::CreateLambda([WeakThis, CompleteDelegate, OriginalResult = Result](const bool bSuccessful, const TOptional<UE::GameFeatures::FResult>& Result)
							{
								CompleteDelegate.ExecuteIfBound(false, OriginalResult);
								if (WeakThis.IsValid())
								{
									for (FOperationCompleted& Delegate : WeakThis->AdditionalActivationDelegate)
									{
										Delegate.ExecuteIfBound(false, OriginalResult);
									}
									WeakThis->AdditionalActivationDelegate.Empty();
								}
							}));
						}
						else // Is we don't have errors, we broadcast the original delegate first, and any other we might have added
						{
							WeakThis->bNeedGameFeatureUnloading = true;
							if (ensure(WeakThis->Status == EGFPakLoaderStatus::ActivatingGameFeature))
							{
								WeakThis->BroadcastOnStatusChange(EGFPakLoaderStatus::GameFeatureActivated); 
							}
							CompleteDelegate.ExecuteIfBound(!Result.HasError(), Result);
							for (FOperationCompleted& Delegate : WeakThis->AdditionalActivationDelegate)
							{
								Delegate.ExecuteIfBound(!Result.HasError(), Result);
							}
							WeakThis->AdditionalActivationDelegate.Empty();
						}
					}
					else // if we are not valid anymore, we still broadcast the delegate
					{
						UE_LOG(LogGFPakLoader, Warning, TEXT("  The pointer to the Pak Plugin '%s' became invalid while the plugin was Activating its GameFeature"), *GFPluginPath)
						CompleteDelegate.ExecuteIfBound(!Result.HasError(), Result);
					}
				}));
			UE_LOG(LogGFPakLoader, Verbose, TEXT("  Called UGameFeaturesSubsystem::LoadAndActivateGameFeaturePlugin for Pak Plugin '%s'..."), *PluginName)
			return;
		}
		else
		{
			UE_LOG(LogGFPakLoader, Error, TEXT("  %s: Unable to retrieve the UGameFeaturesSubsystem. The Pak Plugin will not be registered as a GameFeatures"), *BaseErrorMessage)
		}
	}

	CompleteDelegate.ExecuteIfBound(false, {});
}

void UGFPakPlugin::DeactivateGameFeature_Internal(const FOperationCompleted& CompleteDelegate)
{
	const FString BaseErrorMessage = GetBaseErrorMessage(TEXT("Deactivating"));
	
	if (Status == EGFPakLoaderStatus::DeactivatingGameFeature)
	{
		AdditionalDeactivationDelegate.Add(CompleteDelegate);
		return;
	}
	else if (Status == EGFPakLoaderStatus::Mounted)
	{
		UE_LOG(LogGFPakLoader, Log, TEXT("%s: Trying to Dectivate the GameFeatures of a Pak Plugin that is not Activated."), *BaseErrorMessage)
		CompleteDelegate.ExecuteIfBound(true, {});
		return;
	}
	else if (Status == EGFPakLoaderStatus::ActivatingGameFeature)
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("%s: Trying to Deactivate the GameFeatures of a Pak Plugin that is being Activated. This cannot be cancelled."), *BaseErrorMessage)
		AdditionalActivationDelegate.Add(FOperationCompleted::CreateLambda([WeakThis = TWeakObjectPtr<UGFPakPlugin>(this), CompleteDelegate](const bool bSuccessful, const TOptional<UE::GameFeatures::FResult>& Result)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->DeactivateGameFeature_Internal(CompleteDelegate);
			}
		}));
		return;
	}
	else if (Status != EGFPakLoaderStatus::GameFeatureActivated)
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("%s: Trying to Deactivate the GameFeatures of a Pak Plugin that is not in an Activated state."), *BaseErrorMessage)
		CompleteDelegate.ExecuteIfBound(false, {});
		return;
	}
	
	
	if (bIsGameFeaturesPlugin && GEngine)
	{
		UE_LOG(LogGFPakLoader, Log, TEXT("Deactivating GameFeatures for the Pak Plugin '%s'..."), *PakFilePath)
		if(UGameFeaturesSubsystem* GFSubsystem = GEngine->GetEngineSubsystem<UGameFeaturesSubsystem>())
		{
			BroadcastOnStatusChange(EGFPakLoaderStatus::DeactivatingGameFeature);

			const FString GFPluginPath = UGameFeaturesSubsystem::GetPluginURL_FileProtocol(UPluginPath);

			UE_LOG(LogGFPakLoader, Verbose, TEXT("  Calling UGameFeaturesSubsystem::DeactivateGameFeaturePlugin for Pak Plugin '%s'..."), *PluginName)

			GFSubsystem->DeactivateGameFeaturePlugin(GFPluginPath, FGameFeaturePluginLoadComplete::CreateLambda( // TODO: CALL GFSubsystem->UnloadGameFeaturePlugin on Unmount
				[WeakThis = TWeakObjectPtr<UGFPakPlugin>(this), GFPluginPath, CompleteDelegate](const UE::GameFeatures::FResult& Result)
				{
					UE_CLOG(Result.HasError(), LogGFPakLoader, Error, TEXT("  ... Error while deactivating Pak Plugin '%s':  %s"), *GFPluginPath, *Result.GetError())
					UE_CLOG(!Result.HasError(), LogGFPakLoader, Log, TEXT("  ... Finished deactivating Pak Plugin '%s'"), *GFPluginPath)
					
					if (WeakThis.IsValid())
					{
						if (WeakThis->Status >= EGFPakLoaderStatus::Mounted) // when we are trying to destroy the UGFPakPlugin, we might be already having a `NotInitialized` status for example, and we don't want to replace it
						{
							WeakThis->BroadcastOnStatusChange(EGFPakLoaderStatus::Mounted); // what should happen in case of error? Currently setting back to Mounted to be able to move forward
						}
						CompleteDelegate.ExecuteIfBound(!Result.HasError(), Result);
						for (FOperationCompleted& Delegate : WeakThis->AdditionalDeactivationDelegate)
						{
							Delegate.ExecuteIfBound(!Result.HasError(), Result);
						}
						WeakThis->AdditionalDeactivationDelegate.Empty();
					}
					else
					{
						UE_LOG(LogGFPakLoader, Log, TEXT("  The pointer to the Pak Plugin '%s' became invalid while the plugin was Deactivating its GameFeature"), *GFPluginPath)
						CompleteDelegate.ExecuteIfBound(!Result.HasError(), Result);
					}
				}));
			UE_LOG(LogGFPakLoader, Verbose, TEXT("  Called UGameFeaturesSubsystem::DeactivateGameFeaturePlugin for Pak Plugin '%s'..."), *PluginName)
			return;
		}
		else
		{
			UE_LOG(LogGFPakLoader, Error, TEXT("  %s: Unable to retrieve the UGameFeaturesSubsystem. The Pak Plugin will not be unloaded with the GameFeatureSubsystem"), *BaseErrorMessage)
		}
	}
	
	CompleteDelegate.ExecuteIfBound(false, {});
}

void UGFPakPlugin::UnloadPakPluginObjects_Internal(const FOperationCompleted& CompleteDelegate)
{
	UE_LOG(LogGFPakLoader, Log, TEXT("Unloading Pak Plugin Objects for the Pak Plugin '%s'..."), *PakFilePath)
	const FString BaseErrorMessage = GetBaseErrorMessage(TEXT("Unloading Pak Plugin Objects"));
	
	// We need to ensure there is no rooted package otherwise UnloadGameFeaturePlugin end up calling FGameFeaturePluginState::MarkPluginAsGarbage > UObjectBaseUtility::MarkAsGarbage which crashes if rooted
	// In our case that might happen if the current map is a world from the Pak Plugin being unloaded.
	UWorld* MapWorld = GetWorld();
	if (!ensure(MapWorld))
	{
		UE_LOG(LogGFPakLoader, Error, TEXT(" %s: the World was null"), *BaseErrorMessage)
		CompleteDelegate.ExecuteIfBound(false, {});
		return;
	}
	const UPackage* WorldPackage = MapWorld->GetPackage();
	if (!ensure(WorldPackage))
	{
		UE_LOG(LogGFPakLoader, Error, TEXT(" %s: the World Package was null"), *BaseErrorMessage)
		CompleteDelegate.ExecuteIfBound(false, {});
		return;
	}
	const UGFPakLoaderSubsystem* GFPakLoaderSubsystem = GetSubsystem();
	if (!ensure(GFPakLoaderSubsystem))
	{
		UE_LOG(LogGFPakLoader, Error, TEXT(" %s: the GFPakLoaderSubsystem was null"), *BaseErrorMessage)
		CompleteDelegate.ExecuteIfBound(false, {});
		return;
	}
	
	// 1. First we make sure there are no Rendering resources that might reference the objects we are about to destroy
	FlushRenderingCommands();
	const FNameBuilder WorldPackageName(WorldPackage->GetFName());
	const FStringView WorldPackageMountPointName = FPathViews::GetMountPointNameFromPath(WorldPackageName);
	if (PluginName == WorldPackageMountPointName)
	{
		for (int32 LevelIndex = 0; LevelIndex < MapWorld->GetNumLevels(); LevelIndex++)
		{
			if (ULevel* Level = MapWorld->GetLevel(LevelIndex))
			{
				Level->ReleaseRenderingResources();
			}
		}
	}
	
	// 2. We start destroying the UObjects, Actors and Components
	FlushAsyncLoading();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS); // Needed to ensure LevelStreaming is not trying to Mark objects as Garbage during UPackageTools::UnloadPackages, causing a crash
	FlushRenderingCommands(); // needed to ensure FScene::BatchRemovePrimitives is done removing primitives from RenderThread
	PurgePakPluginContent(PluginName, [](UObject* Object)
	{
		return !(Object->IsA(UGameFeatureData::StaticClass()) || Object->IsA(UWorld::StaticClass()) || Object->IsA(ULevel::StaticClass()) || Object->IsA(AWorldSettings::StaticClass()));
	}, false); //mostly remove them from Root

	// 3. We need to check if we are needing to unload the current map and if yes, if we need to open a new map
	const bool bIsShuttingDown = GFPakLoaderSubsystem->IsShuttingDown();
	bool bNeedToOpenNewLevel = false;
	if (PluginName == WorldPackageMountPointName)
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("  %s: The current World is from the GFPakPlugin being Unmounted! Reloading the default Map ..."), *BaseErrorMessage);
		const UGameMapsSettings* GameMapsSettings = GetDefault<UGameMapsSettings>();
		if (bIsShuttingDown)
		{
			UE_LOG(LogGFPakLoader, Warning, TEXT("  SHUTTING DOWN WAS REQUESTED, NOT TRYING TO LOAD A NEW WORLD"))
		}
		else
		{
			UE_LOG(LogGFPakLoader, Log, TEXT("Opening the Map '%s'"), *GameMapsSettings->GetGameDefaultMap())
			UGameplayStatics::OpenLevel(this, FName(GameMapsSettings->GetGameDefaultMap()), false);
			bNeedToOpenNewLevel = true;
		}
	}

	// 4. Creation of the unloading lambdas
	FString GFPluginPath = UGameFeaturesSubsystem::GetPluginURL_FileProtocol(UPluginPath);
	auto UnloadGameFeature = [WeakThis = TWeakObjectPtr<UGFPakPlugin>(this), bNeedGameFeatureUnloading = bNeedGameFeatureUnloading, PluginName = PluginName, GFPluginPath = MoveTemp(GFPluginPath),
		BackupMountPoints = PakPluginMountPoints, CompleteDelegate]() mutable
	{
		UE_LOG(LogGFPakLoader, Log, TEXT("UnloadGameFeature"))
		UE_LOG(LogGFPakLoader, Log, TEXT("FinishDestroyingObjects"))
		PurgePakPluginContent(PluginName, [](const UObject* Object)
		{
			return !(Object->IsA(UGameFeatureData::StaticClass()) || Object->IsA(UWorld::StaticClass()) || Object->IsA(ULevel::StaticClass()) || Object->IsA(AWorldSettings::StaticClass()));
		});
		
		UGameFeaturesSubsystem* GFSubsystem = GEngine ? GEngine->GetEngineSubsystem<UGameFeaturesSubsystem>() : nullptr;
		if (GFSubsystem && bNeedGameFeatureUnloading)
		{
			GFSubsystem->UnloadGameFeaturePlugin(GFPluginPath, FGameFeaturePluginLoadComplete::CreateLambda(
		[GFPluginPath, PluginName, BackupMountPoints = MoveTemp(BackupMountPoints), CompleteDelegate](const UE::GameFeatures::FResult& Result)
			{
				UE_CLOG(Result.HasError(), LogGFPakLoader, Error, TEXT("  ... Error while unloading Pak Plugin '%s':  %s"), *GFPluginPath, *Result.GetError())
				UE_CLOG(!Result.HasError(), LogGFPakLoader, Log, TEXT("  ... Finished unloading Pak Plugin '%s'"), *GFPluginPath)
				UE_LOG(LogGFPakLoader, Log, TEXT("PostUnloadGameFeature"))
				PurgePakPluginContent(PluginName, [](UObject*){ return true;});
				CompleteDelegate.ExecuteIfBound(!Result.HasError(), Result);
			}));
		}
		else
		{
			UE_LOG(LogGFPakLoader, Log, TEXT("PostUnloadGameFeature"))
			PurgePakPluginContent(PluginName, [](UObject*){ return true;});
			CompleteDelegate.ExecuteIfBound(true, {});
		}
	};
	
	// Mount Points need to be unregistered after the GameFeatures Deactivating
	if (bNeedToOpenNewLevel)
	{
		// Here we just want to make sure we register to the PostLoadMapWithWorld only once
		struct FHandleHolder
		{
			TOptional<FDelegateHandle> Handle = {};
		};
		TSharedPtr<FHandleHolder> HandleHolder = MakeShared<FHandleHolder>();
		HandleHolder->Handle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda([HandleHolder, UnloadGameFeature = MoveTemp(UnloadGameFeature)](UWorld* World) mutable
		{
			UE_LOG(LogGFPakLoader, Log, TEXT("PostLoadMapWithWorld"))
			UnloadGameFeature();
			if (HandleHolder && HandleHolder->Handle)
			{
				FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(HandleHolder->Handle.GetValue());
			}
		});
	}
	else
	{
		UnloadGameFeature();
	}
	UE_LOG(LogGFPakLoader, Verbose, TEXT("  Called UGameFeaturesSubsystem::UnloadPakPluginObjects_Internal for Pak Plugin '%s'..."), *PluginName)
}

bool UGFPakPlugin::Unmount_Internal()
{
	const FString BaseErrorMessage = GetBaseErrorMessage(TEXT("Unmounting"));
	
	if (Status < EGFPakLoaderStatus::Mounted)
	{
		UE_LOG(LogGFPakLoader, Log, TEXT("%s: Trying to unmount a Pak Plugin that is not in a Mounted state."), *BaseErrorMessage)
		return Status == EGFPakLoaderStatus::Unmounted;
	}

	UE_LOG(LogGFPakLoader, Log, TEXT("Unmounting the Pak Plugin '%s'..."), *PakFilePath)
	if (PluginInterface)
	{
		FText FailReason;
		if (UnloadPlugin(PluginInterface.ToSharedRef(), &FailReason))
		{
			UE_LOG(LogGFPakLoader, Log, TEXT("  UPlugin '%s' unloaded"), *PluginName)
		}
		else
		{
			UE_LOG(LogGFPakLoader, Error, TEXT("  %s: Unable to unload the Plugin '%s':  '%s'"), *BaseErrorMessage, *PluginName, *FailReason.ToString())
		}
		PluginInterface = nullptr;
	}
	UnloadPakPluginObjects_Internal(FOperationCompleted::CreateLambda([WeakThis = TWeakObjectPtr<UGFPakPlugin>(this), PakFilePath = PakFilePath]
		(const bool bSuccessful, const TOptional<UE::GameFeatures::FResult>& Result)
	{
		if (WeakThis.IsValid())
		{
			if (WeakThis->Status >= EGFPakLoaderStatus::Unmounted) // when we are trying to destroy the UGFPakPlugin, we might be already having a `NotInitialized` status for example, and we don't want to replace it
			{
				WeakThis->BroadcastOnStatusChange(EGFPakLoaderStatus::Unmounted); // what should happen in case of error? Currently setting back to Mounted to be able to move forward
			}
		}
		else
		{
			UE_LOG(LogGFPakLoader, Log, TEXT("  The pointer to the Pak Plugin '%s' became invalid while the plugin was Unloading its Objects"), *PakFilePath)
		}
	}));
	PakPluginMountPoints.Empty();
	
	FGFPakLoaderPlatformFile* PakPlatformFile = GetSubsystem() ? GetSubsystem()->GetGFPakPlatformFile() : nullptr; // We need to ensure the PakPlatformFile is loaded or the following might not work
	if (!ensure(PakPlatformFile) || !FCoreDelegates::OnUnmountPak.IsBound())
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("  %s: The Unmounting Delegate is not bound. Not able to Unmount the Pak file."), *BaseErrorMessage)
		return false;
	}
	PakPlatformFile->InitializeNewAsyncIO(); // this ensures that the FPakPrecacher Singleton is valid, as it might have been deleted at this point in Game builds
	if (!ensure(FCoreDelegates::OnUnmountPak.Execute(PakFilePath)))
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("  %s: Unable to unmount"), *BaseErrorMessage);
	}
	UE_LOG(LogGFPakLoader, Log, TEXT("  Unmounted the Pak Plugin '%s'"), *PakFilePath)

	MountedPakFile = nullptr;
	PluginAssetRegistry.Reset();

	BroadcastOnStatusChange(EGFPakLoaderStatus::Unmounted);
	
	return true;
}

void UGFPakPlugin::Deinitialize_Internal()
{
	if (Status >= EGFPakLoaderStatus::Mounted)
	{
		Unmount_Internal();
	}

	UE_CLOG(Status != EGFPakLoaderStatus::NotInitialized && Status != EGFPakLoaderStatus::InvalidPluginDirectory, LogGFPakLoader, Log, TEXT("Deinitialized the Pak Plugin '%s'"), *PakPluginDirectory)
	PluginName.Empty();
	PakFilePath.Empty();
	UPluginPath.Empty();
	PluginDescriptor = {};
	bIsGameFeaturesPlugin = false;
	MountedPakFile = nullptr;
	PluginAssetRegistry.Reset();
	BroadcastOnStatusChange(EGFPakLoaderStatus::NotInitialized);
}

void UGFPakPlugin::PurgePakPluginContent(const FString& PakPluginName, const TFunctionRef<bool(UObject*)>& ShouldObjectBePurged, bool bMarkAsGarbage)
{
	// Inspired by FPackageMigrationContext::CleanInstancedPackages and FDataprepCoreUtils::PurgeObjects
	TArray<UObject*> ObjectsToPurge;
	TArray<UObject*> PublicObjectsToPurge; // Objects with the RF_Public flag needs to be handled slightly differently
	
	FGlobalComponentReregisterContext ComponentContext;
	
	for (TObjectIterator<UPackage> It; It; ++It)
	{
		UPackage* Package = *It;
		if (!IsValid(Package))
		{
			continue;
		}
		const FNameBuilder PackageName(Package->GetFName());
		const FStringView PackageMountPointName = FPathViews::GetMountPointNameFromPath(PackageName);
		if (PakPluginName == PackageMountPointName) //todo: is there a better way to figure out if this is from this plugin?
		{
			const ERenameFlags PkgRenameFlags = REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional | REN_SkipGeneratedClasses;
			check(Package->Rename(*MakeUniqueObjectName(nullptr, UPackage::StaticClass(), *FString::Printf(TEXT("%s_PAKPLUGINUNLOADED"), *Package->GetName())).ToString(), nullptr, PkgRenameFlags));
			if (PurgeObject(Package, ShouldObjectBePurged, ObjectsToPurge, PublicObjectsToPurge, bMarkAsGarbage))
			{
				if (FLinkerLoad* Linker = Package->GetLinker()) 
				{
					// if we do not null the Linker, the call to UPackageTools::UnloadPackages will call
					// FLinkerManager::ResetLoaders > FLinkerLoad::LoadAndDetachAllBulkData which will fail loading the bulk data and crash the engine
					Linker->Detach();
				}
			}
		}
	}
	
	/** 
	 * If we have any public object that were made purgeable, null out their references so we can safely garbage collect
	 * Additionally, ObjectTools::ForceReplaceReferences is calling PreEditChange and PostEditChange on all impacted objects.
	 * Consequently, making sure async tasks processing those objects are notified and act accordingly.
	 * This is the way to make sure that all dependencies are taken in account and properly handled
	 */
#if WITH_EDITOR
	if (bMarkAsGarbage && PublicObjectsToPurge.Num() > 0)
	{
		/**
		 * Due to way that some render proxy are created we must remove the current rendering scene.
		 * This is to ensure that the render proxies won't have a dangling pointer to an asset while removing then on the next tick
		 */
		FGlobalComponentRecreateRenderStateContext RefreshRendering;
		ObjectTools::ForceReplaceReferences(nullptr, PublicObjectsToPurge);

		// Ensure that all the rendering commands were processed before doing the garbage collection (see above comment)
		FlushRenderingCommands();
	}
#endif // WITH_EDITOR
	// if we have object to purge but the map isn't one of them collect garbage (if we purged the map it has already been done)
	if ( ObjectsToPurge.Num() > 0 )
	{
		FlushRenderingCommands();

		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		FlushRenderingCommands();
	}
}

bool UGFPakPlugin::PurgeObject(UObject* Object, const TFunctionRef<bool(UObject*)>& ShouldObjectBePurged, TArray<UObject*>& ObjectsToPurge, TArray<UObject*>& PublicObjectsToPurge, bool bMarkAsGarbage)
{
	if (IsValid(Object))
	{
		if (!ShouldObjectBePurged(Object))
		{
			return false;
		}

		{
			bool bAnyInnerObjectNotPurged = false;
			ForEachObjectWithOuter(Object, [&bAnyInnerObjectNotPurged, &ObjectsToPurge, &PublicObjectsToPurge, &ShouldObjectBePurged, bMarkAsGarbage](UObject* Obj)
			{
				bAnyInnerObjectNotPurged &= PurgeObject(Obj, ShouldObjectBePurged, ObjectsToPurge, PublicObjectsToPurge, bMarkAsGarbage);
			});
			if (bAnyInnerObjectNotPurged)
			{
				return false;
			}
		}

#if WITH_EDITOR
		/**
		 * Add object for reference removal if it's public
		 * This is used to emulate the workflow that is used by the editor when deleting a asset.
		 * Due to the transient package we can't simply use IsAsset()
		 */
		if (Object->HasAnyFlags( RF_Public ) )
		{
			PublicObjectsToPurge.Add(Object);
		}
		if (Object->IsAsset())
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Object);
		}
#endif // WITH_EDITOR
		
		if (Object->IsRooted())
		{
			Object->RemoveFromRoot();
		}

		Object->ClearFlags(RF_Public | RF_Standalone);
		if (bMarkAsGarbage)
		{
			Object->MarkAsGarbage();
		}
		ObjectsToPurge.Add(Object);
	}
	return true;
}

TSharedPtr<IPlugin> UGFPakPlugin::LoadPlugin(const FString& PluginFilePath, FText* OutFailReason)
{
	if (!FPaths::FileExists(PluginFilePath))
	{
		if (OutFailReason)
		{
			*OutFailReason = FText::Format(LOCTEXT("PluginFileDoesNotExist", "Plugin file does not exist\n{0}"), FText::FromString(FPaths::ConvertRelativePathToFull(PluginFilePath)));
		}
		return nullptr;
	}
	
	const FString PluginName = FPaths::GetBaseFilename(PluginFilePath);
	const FString PluginLocation = FPaths::GetPath(FPaths::GetPath(PluginFilePath));
	// return PluginUtils::LoadPluginInternal(PluginName, PluginLocation, PluginFileName, LoadParams, /*bIsNewPlugin*/ false);
	
	if (!IPluginManager::Get().AddToPluginsList(PluginFilePath, OutFailReason))
	{
		return nullptr;
	}

	// Find the plugin in the manager.
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
	if (!Plugin)
	{
		if (OutFailReason)
		{
			*OutFailReason = FText::Format(LOCTEXT("FailedToRegisterPlugin", "Failed to register plugin\n{0}"), FText::FromString(PluginFilePath));
		}
		return nullptr;
	}

	// Double check the path matches
	if (!FPaths::IsSamePath(Plugin->GetDescriptorFileName(), PluginFilePath))
	{
		if (OutFailReason)
		{
			const FString PluginFilePathFull = FPaths::ConvertRelativePathToFull(Plugin->GetDescriptorFileName());
			*OutFailReason = FText::Format(LOCTEXT("PluginNameAlreadyUsed", "There's already a plugin named {0} at this location:\n{1}"), FText::FromString(PluginName), FText::FromString(PluginFilePathFull));
		}
		return nullptr;
	}
	
	const FString PluginRootFolder = Plugin->CanContainContent() ? Plugin->GetMountedAssetPath() : FString();
	bool bOutAlreadyLoaded = Plugin->IsEnabled() && (PluginRootFolder.IsEmpty() || FPackageName::MountPointExists(PluginRootFolder));
	if (!bOutAlreadyLoaded)
	{
		// Mount the new plugin (mount content folder if any and load modules if any)
		IPluginManager::Get().MountExplicitlyLoadedPlugin(PluginName);
		if (!Plugin->IsEnabled())
		{
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("FailedToEnablePlugin", "Failed to enable plugin because it is not configured as bExplicitlyLoaded=true\n{0}"), FText::FromString(PluginFilePath));
			}
			return nullptr;
		}
	}

	return Plugin;
}

bool UGFPakPlugin::UnloadPlugin(const TSharedRef<IPlugin>& Plugin, FText* OutFailReason)
{
	const TConstArrayView<TSharedRef<IPlugin>> Plugins {Plugin};
	{
		FText ErrorMsg;
		if (!UnloadPluginsAssets(Plugins, &ErrorMsg))
		{
			// If some assets fail to unload, log an error, but unmount the plugins anyway
			UE_LOG(LogGFPakLoader, Error, TEXT("Failed to unload some assets prior to unmounting plugins\n%s"), *ErrorMsg.ToString());
		}
	}
	
	// Unmount the plugins
	//
	bool bSuccess = true;
	{
		FTextBuilder ErrorBuilder;
		bool bPluginUnmounted = false;
		
		if (Plugin->IsEnabled())
		{
			bPluginUnmounted = true;

			FText FailReason;
			if (!IPluginManager::Get().UnmountExplicitlyLoadedPlugin(Plugin->GetName(), &FailReason))
			{
				UE_LOG(LogGFPakLoader, Error, TEXT("Plugin %s cannot be unloaded: %s"), *Plugin->GetName(), *FailReason.ToString());
				ErrorBuilder.AppendLine(FailReason);
				bSuccess = false;
			}
		}
		

		if (bPluginUnmounted)
		{
			IPluginManager::Get().RefreshPluginsList();
		}

		if (!bSuccess && OutFailReason)
		{
			*OutFailReason = ErrorBuilder.ToText();
		}
	}
	return bSuccess;
}

bool UGFPakPlugin::UnloadPluginsAssets(const TConstArrayView<TSharedRef<IPlugin>> Plugins, FText* OutFailReason)
{
	TSet<FString> PluginNames;
	PluginNames.Reserve(Plugins.Num());
	for (const TSharedRef<IPlugin>& Plugin : Plugins)
	{
		PluginNames.Add(Plugin->GetName());
	}

	return UnloadPluginsAssets(PluginNames, OutFailReason);
}

bool UGFPakPlugin::UnloadPluginsAssets(const TSet<FString>& PluginNames, FText* OutFailReason)
{
	bool bSuccess = true;
	if (!PluginNames.IsEmpty())
	{
		const double StartTime = FPlatformTime::Seconds();

		TArray<UPackage*> PackagesToUnload;
		for (TObjectIterator<UPackage> It; It; ++It)
		{
			const FNameBuilder PackageName(It->GetFName());
			const FStringView PackageMountPointName = FPathViews::GetMountPointNameFromPath(PackageName);
			if (PluginNames.ContainsByHash(GetTypeHash(PackageMountPointName), PackageMountPointName))
			{
				PackagesToUnload.Add(*It);
			}
		}

		if (PackagesToUnload.Num() > 0)
		{
			FText ErrorMsg;
			UnloadPackages(PackagesToUnload, ErrorMsg, /*bUnloadDirtyPackages=*/true);

			// @note UnloadPackages returned bool indicates whether some packages were unloaded
			// To tell whether all packages were successfully unloaded we must check the ErrorMsg output param
			if (!ErrorMsg.IsEmpty())
			{
				if (OutFailReason)
				{
					*OutFailReason = MoveTemp(ErrorMsg);
				}
				bSuccess = false;
			}
		}

		UE_LOG(LogGFPakLoader, Log, TEXT("Unloading assets from %d plugins took %0.2f sec"), PluginNames.Num(), FPlatformTime::Seconds() - StartTime);
	}
	return bSuccess;
}


#if WITH_EDITOR
/** State passed to RestoreStandaloneOnReachableObjects. */
TSet<UPackage*>* UGFPakPlugin::PackagesBeingUnloaded = nullptr;
TSet<UObject*> UGFPakPlugin::ObjectsThatHadFlagsCleared;
FDelegateHandle UGFPakPlugin::ReachabilityCallbackHandle;
#endif


bool UGFPakPlugin::UnloadPackages(const TArray<UPackage*>& Packages, FText& OutErrorMessage, bool bUnloadDirtyPackages)
{
	// Early out if no package is provided
	if (Packages.IsEmpty())
	{
		return true;
	}

	bool bResult = false;

	// Get outermost packages, in case groups were selected.
	TSet<UPackage*> PackagesToUnload;
	
	// Split the set of selected top level packages into packages which are dirty (and thus cannot be unloaded)
	// and packages that are not dirty (and thus can be unloaded).
	TArray<UPackage*> DirtyPackages;
	for (UPackage* TopLevelPackage : Packages)
	{
		if (TopLevelPackage)
		{
			if (!bUnloadDirtyPackages && TopLevelPackage->IsDirty())
			{
				DirtyPackages.Add(TopLevelPackage);
			}
			else
			{
				UPackage* PackageToUnload = TopLevelPackage->GetOutermost();
				if (!PackageToUnload)
				{
					PackageToUnload = TopLevelPackage;
				}
				PackagesToUnload.Add(PackageToUnload);
			}
		}
	}
	// Inform the user that dirty packages won't be unloaded.
	if ( DirtyPackages.Num() > 0 )
	{
		FString DirtyPackagesList;
		for (UPackage* DirtyPackage : DirtyPackages)
		{
			DirtyPackagesList += FString::Printf(TEXT("\n    %s"), *(DirtyPackage->GetName()));
		}

		FFormatNamedArguments Args;
		Args.Add(TEXT("DirtyPackages"), FText::FromString(DirtyPackagesList));

		OutErrorMessage = FText::Format( LOCTEXT("UnloadDirtyPackagesList", "The following assets have been modified and cannot be unloaded:{DirtyPackages}\nSaving these assets will allow them to be unloaded."), Args );
	}
#if WITH_EDITOR
	if (GEditor)
	{
		if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
		{
			// Is the current world being unloaded?
			if (PackagesToUnload.Contains(EditorWorld->GetPackage()))
			{
				TArray<TWeakObjectPtr<UPackage>> WeakPackages;
				WeakPackages.Reserve(PackagesToUnload.Num());
				for (UPackage* Package : PackagesToUnload)
				{
					WeakPackages.Add(Package);
				}

				// Unload the current world
				GEditor->CreateNewMapForEditing();

				// Remove stale entries in PackagesToUnload (unloaded world, level build data, streaming levels, external actors, etc)
				PackagesToUnload.Reset();
				for (const TWeakObjectPtr<UPackage>& WeakPackage : WeakPackages)
				{
					if (UPackage* Package = WeakPackage.Get())
					{
						PackagesToUnload.Add(Package);
					}
				}
			}
		}
	}
	else
	{
		return bResult;
	}
#endif

	if (PackagesToUnload.Num() > 0)
	{
		// Complete any load/streaming requests, then lock IO.
		FlushAsyncLoading();
		(*GFlushStreamingFunc)();

		GWarn->BeginSlowTask(NSLOCTEXT("UnrealEd", "Unloading", "Unloading"), true);

#if WITH_EDITOR
		if (GEditor)
		{
			// Remove potential references to to-be deleted objects from the GB selection set.
			GEditor->GetSelectedObjects()->GetElementSelectionSet()->ClearSelection(FTypedElementSelectionOptions());

			// Clear undo history because transaction records can hold onto assets we want to unload
			if (GEditor->Trans) //  && Params.bResetTransBuffer
			{
				GEditor->Trans->Reset(LOCTEXT("UnloadPackagesResetUndo", "Unload Assets"));
			}
		}
#endif
		// First add all packages to unload to the root set so they don't get garbage collected while we are operating on them
		TArray<UPackage*> PackagesAddedToRoot;
		for (UPackage* PackageToUnload : PackagesToUnload)
		{
			if (!PackageToUnload->IsRooted())
			{
				PackageToUnload->AddToRoot();
				PackagesAddedToRoot.Add(PackageToUnload);
			}
		}
#if WITH_EDITOR
		// We need to make sure that there is no async compilation work running for the packages that we are about to unload
		// so that it is safe to call ::ResetLoaders
		UPackageTools::FlushAsyncCompilation(PackagesToUnload.Array());
#endif
		
		// Now try to clean up assets in all packages to unload.
		bool bScriptPackageWasUnloaded = false;
		int32 PackageIndex = 0;
		for (UPackage* PackageBeingUnloaded : PackagesToUnload)
		{
			GWarn->StatusUpdate(PackageIndex++, PackagesToUnload.Num(), FText::Format(LOCTEXT("Unloadingf", "Unloading {0}..."), FText::FromString(PackageBeingUnloaded->GetName()) ) );

			// Flush all pending render commands, as unloading the package may invalidate render resources.
			FlushRenderingCommands();

			TArray<UObject*> ObjectsInPackage;

#if WITH_EDITOR
			// Can't use ForEachObjectWithPackage here as closing the editor may modify UObject hash tables (known case: renaming objects)
			GetObjectsWithPackage(PackageBeingUnloaded, ObjectsInPackage, false);
			// Close any open asset editors.
			for (UObject* Obj : ObjectsInPackage)
			{
				if (Obj->IsAsset())
				{
					GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Obj);
				}
			}
			ObjectsInPackage.Reset();

			PackageBeingUnloaded->MarkAsUnloaded();
#endif
			if ( PackageBeingUnloaded->HasAnyPackageFlags(PKG_ContainsScript) )
			{
				bScriptPackageWasUnloaded = true;
			}

			GetObjectsWithPackage(PackageBeingUnloaded, ObjectsInPackage, true, RF_Transient, EInternalObjectFlags::Garbage);
			// Notify any Blueprints and other systems that are about to be unloaded, also destroy any leftover worlds.
			for (UObject* Obj : ObjectsInPackage)
			{
				// Asset manager can hold hard references to this object and prevent GC
				if (true) //PackageTools_Private::bUnloadPackagesUnloadsPrimaryAssets
				{
					const FPrimaryAssetId PrimaryAssetId = UAssetManager::Get().GetPrimaryAssetIdForObject(Obj);
					if (PrimaryAssetId.IsValid())
					{
						UAssetManager::Get().UnloadPrimaryAsset(PrimaryAssetId);
					}
				}

#if WITH_EDITOR
				if (UBlueprint* BP = Cast<UBlueprint>(Obj))
				{
					BP->ClearEditorReferences();

					// Remove from cached dependent lists.
					for (const TWeakObjectPtr<UBlueprint> Dependency : BP->CachedDependencies)
					{
						if (UBlueprint* ResolvedDependency = Dependency.Get())
						{
							ResolvedDependency->CachedDependents.Remove(BP);
						}
					}

					BP->CachedDependencies.Reset();

					// Remove from cached dependency lists.
					for (const TWeakObjectPtr<UBlueprint> Dependent : BP->CachedDependents)
					{
						if (UBlueprint* ResolvedDependent = Dependent.Get())
						{
							ResolvedDependent->CachedDependencies.Remove(BP);
						}
					}

					BP->CachedDependents.Reset();
				}
				else if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Obj))
				{
					FKismetEditorUtilities::OnBlueprintGeneratedClassUnloaded.Broadcast(BPGC);
				}
				else
#endif
				if (UWorld* World = Cast<UWorld>(Obj))
				{
					if (World->bIsWorldInitialized)
					{
						World->CleanupWorld();
					}
				}
			}
			ObjectsInPackage.Reset();

			// Clear RF_Standalone flag from objects in the package to be unloaded so they get GC'd.
			{
				GetObjectsWithPackage(PackageBeingUnloaded, ObjectsInPackage);
				for ( UObject* Object : ObjectsInPackage )
				{
					if (Object->HasAnyFlags(RF_Standalone))
					{
						Object->ClearFlags(RF_Standalone);
#if WITH_EDITOR
						ObjectsThatHadFlagsCleared.Add(Object);
#endif
					}
				}
				ObjectsInPackage.Reset();
			}

			// Cleanup.
			bResult = true;
		}

		// Calling ::ResetLoaders now will force any bulkdata objects still attached to the FLinkerLoad to load
		// their payloads into memory. If we don't call this now, then the version that will be called during
		// garbage collection will cause the bulkdata objects to be invalidated rather than loading the payloads 
		// into memory.
		// This might seem odd, but if the package we are unloading is being renamed, then the inner UObjects will
		// be moved to the newly named package rather than being garbage collected and so we need to make sure that
		// their bulkdata objects remain valid, otherwise renamed packages will not save correctly and cease to function.
		ResetLoaders(TArray<UObject*>(PackagesToUnload.Array()));

		for (UPackage* PackageBeingUnloaded : PackagesToUnload)
		{
			if (PackageBeingUnloaded->IsDirty())
			{
				// The package was marked dirty as a result of something that happened above (e.g callbacks in CollectGarbage).  
				// Dirty packages we actually care about unloading were filtered above so if the package becomes dirty here it should still be unloaded
				PackageBeingUnloaded->SetDirtyFlag(false);
			}
		}

#if WITH_EDITOR
		// Set the callback for restoring RF_Standalone post reachability analysis.
		// GC will call this function before purging objects, allowing us to restore RF_Standalone
		// to any objects that have not been marked RF_Unreachable.
		ReachabilityCallbackHandle = FCoreUObjectDelegates::PostReachabilityAnalysis.AddStatic(RestoreStandaloneOnReachableObjects);

		PackagesBeingUnloaded = &PackagesToUnload;
#endif
		// Collect garbage.
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
#if WITH_EDITOR
		ObjectsThatHadFlagsCleared.Empty();
		PackagesBeingUnloaded = nullptr;
#endif
		// Now remove from root all the packages we added earlier so they may be GCed if possible
		for (UPackage* PackageAddedToRoot : PackagesAddedToRoot)
		{
			PackageAddedToRoot->RemoveFromRoot();
		}
		PackagesAddedToRoot.Empty();

		GWarn->EndSlowTask();

#if WITH_EDITOR
		// Remove the post reachability callback.
		FCoreUObjectDelegates::PostReachabilityAnalysis.Remove(ReachabilityCallbackHandle);
#endif

#if WITH_EDITORONLY_DATA
		// Clear the standalone flag on metadata objects that are going to be GC'd below.
		// This resolves the circular dependency between metadata and packages.
		TArray<TWeakObjectPtr<UMetaData>> PackageMetaDataWithClearedStandaloneFlag;
		for (UPackage* PackageToUnload : PackagesToUnload)
		{
			UMetaData* PackageMetaData = PackageToUnload ? PackageToUnload->GetMetaData() : nullptr;
			if ( PackageMetaData && PackageMetaData->HasAnyFlags(RF_Standalone) )
			{
				PackageMetaData->ClearFlags(RF_Standalone);
				PackageMetaDataWithClearedStandaloneFlag.Add(PackageMetaData);
			}
		}
#endif
		
		CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
		FlushRenderingCommands();
		
#if WITH_EDITORONLY_DATA
		// Restore the standalone flag on any metadata objects that survived the GC
		for ( const TWeakObjectPtr<UMetaData>& WeakPackageMetaData : PackageMetaDataWithClearedStandaloneFlag )
		{
			UMetaData* MetaData = WeakPackageMetaData.Get();
			if ( MetaData )
			{
				MetaData->SetFlags(RF_Standalone);
			}
		}
#endif
		
#if WITH_EDITOR
		// Update the actor browser if a script package was unloaded
		if ( bScriptPackageWasUnloaded )
		{
			GEditor->BroadcastClassPackageLoadedOrUnloaded();
		}
#endif
	}
	return bResult;
}

#if WITH_EDITOR
void UGFPakPlugin::RestoreStandaloneOnReachableObjects()
{
	check(GIsEditor);

	if (PackagesBeingUnloaded && ObjectsThatHadFlagsCleared.Num() > 0)
	{
		for (UPackage* PackageBeingUnloaded : *PackagesBeingUnloaded)
		{
			ForEachObjectWithPackage(PackageBeingUnloaded, [](UObject* Object)
			{
				if (ObjectsThatHadFlagsCleared.Contains(Object))
				{
					Object->SetFlags(RF_Standalone);
				}
				return true;
			}, true, RF_NoFlags, UE::GC::GUnreachableObjectFlag);
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE
