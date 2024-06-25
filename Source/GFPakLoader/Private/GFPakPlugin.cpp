// Copyright GeoTech BV


#include "GFPakPlugin.h"

#include "ComponentReregisterContext.h"
#include "GameFeatureData.h"
#include "GameFeaturePluginOperationResult.h"
#include "GameFeaturesSubsystem.h"
#include "GFPakLoaderDirectoryVisitors.h"
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
#include "Misc/CoreDelegates.h"
#include "Misc/PathViews.h"
#include "UObject/LinkerLoad.h"

#if WITH_EDITOR
#include "ComponentRecreateRenderStateContext.h"
#include "ContentBrowserDataSubsystem.h"
#include "IContentBrowserDataModule.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "Selection.h"
#include "Algo/AnyOf.h"
#include "Editor/Transactor.h"
#include "Elements/Interfaces/TypedElementSelectionInterface.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

#define LOCTEXT_NAMESPACE "FGFPakLoaderModule"


FGFPakFilenameMap FGFPakFilenameMap::FromFilenameAndMountPoints(const FString& OriginalMountPoint, const FString& AdjustedMountPoint, const FString& OriginalFilename)
{
	FGFPakFilenameMap PakFilename;
	PakFilename.OriginalMountPoint = OriginalMountPoint;
	PakFilename.OriginalFilename = OriginalFilename; //FPaths::GetBaseFilename(OriginalFilename, false);
	PakFilename.AdjustedMountPoint = AdjustedMountPoint;
	
	PakFilename.OriginalFullFilename = OriginalMountPoint + PakFilename.OriginalFilename;
	PakFilename.AdjustedFullFilename = AdjustedMountPoint + PakFilename.OriginalFilename;
	
	PakFilename.ProjectAdjustedFullFilename = PakFilename.OriginalFullFilename;
	// Default Mount Point is "../../../<DLCProjectOrEngine>/Content/<folder>/filename.uasset" or "../../../<DLCProject>/Plugins[/GameFeatures]/<folder>/filename.uasset", and we want to make sure the "<DLCProject>" is matching the current project to find the current Mount Path
	// So for example, if the DLC was packaged from the project "DLCProject", the path would be "../../../DLCProject/Content/<folder>/filename.uasset", but we want a path adjusted to the current project "../../../CurrentProject/Content/<folder>/filename.uasset"
	//todo: test with Engine content
	if (ensure(PakFilename.ProjectAdjustedFullFilename.RemoveFromStart(TEXT("../../../")))) 
	{
		// here, the filename should be "<DLCProjectOrEngine>/Content/<folder>/filename.uasset"
		FString DLCProjectName;
		FString PathAfterProject;
		if (PakFilename.ProjectAdjustedFullFilename.Split(TEXT("/"), &DLCProjectName, &PathAfterProject) && DLCProjectName != TEXT("Engine")) //todo: test with DLC engine/editor content
		{
			// here, the ProjectName should be "<DLCProject>" and PathAfterProject should be "Content/<folder>/filename.uasset" or "Plugins[/GameFeatures]/Content/<folder>/filename.uasset"
			FString FullProjectDir = FPlatformMisc::ProjectDir(); // ex: "D:/<folder>/CurrentProject/" if in other drive, otherwise "../../../../../../<folder>/CurrentProject/" for example
			FString ProjectDir = FPaths::ConvertRelativePathToFull(FullProjectDir); // ex: "D:/<folder>/CurrentProject/" if in other drive, otherwise "C:/<folder>/CurrentProject/" for example
			FPaths::MakePathRelativeTo(ProjectDir, FPlatformProcess::BaseDir()); // Makes it relative to match. ex: "D:/<folder>/CurrentProject/" if in other drive, otherwise "../../../../../../<folder>/CurrentProject/" for example
			PakFilename.ProjectAdjustedFullFilename = ProjectDir / PathAfterProject; // Replace the project name of the pak with the current project name
		}
		else
		{
			// here, the filename should be an Engine content, so we just add back what we removed "Engine/Content/<folder>/filename.uasset" => "../../../Engine/<folder>/filename.uasset"
			PakFilename.ProjectAdjustedFullFilename = TEXT("../../../") + PakFilename.ProjectAdjustedFullFilename;
		}
	}
	// here, the ProjectAdjustedFullFilename should be "../../Content/<folder>/filename.uasset"

	PakFilename.MountedPackageName = NAME_None;
	PakFilename.LocalBaseFilename = NAME_None;
	TStringBuilder<64> MountPointName;
	TStringBuilder<256> MountPointPath;
	TStringBuilder<256> RelativePath;
	if (FPackageName::TryGetMountPointForPath(PakFilename.ProjectAdjustedFullFilename, MountPointName, MountPointPath,RelativePath))
	{
		FPackagePath Path;
		if (FPackagePath::TryFromMountedName(PakFilename.ProjectAdjustedFullFilename, Path))
		{
			PakFilename.MountedPackageName = Path.GetPackageFName();
		}
		PakFilename.LocalBaseFilename = FName(FString(MountPointPath) + RelativePath); // not using Path.GetLocalFullPath as the .uexp will create warnings during FPackagePath::FromMountedComponents
	}
	
	UE_LOG(LogGFPakLoader, VeryVerbose, TEXT("  FGFPakFilenameMap::FromFilenameAndMountPoints '%s %s' => '%s' (Mounted to '%s' => '%s')"),
		*OriginalMountPoint, *OriginalFilename, *PakFilename.ProjectAdjustedFullFilename,
		*PakFilename.MountedPackageName.ToString(), *PakFilename.LocalBaseFilename.ToString());
	
	return PakFilename;
}



void UGFPakPlugin::BeginDestroy()
{
	OnBeginDestroyDelegate.Broadcast(this);
	
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
	OnDestroyedDelegate.Broadcast(this);
	
	UObject::BeginDestroy();
}

bool UGFPakPlugin::LoadPluginData()
{
	const bool Result = LoadPluginData_Internal();
	BroadcastOnStatusChange(Status);

	if (UGFPakLoaderSubsystem::GetPakLoaderSettings()->bAutoMountPakPlugins && Status == EGFPakLoaderStatus::Unmounted)
	{
		Mount(); //should we return this instead?
	}
	return Result;
}

bool UGFPakPlugin::Mount()
{
	const bool Result = Mount_Internal();
	BroadcastOnStatusChange(Status);
	if (bHasUPlugin && bIsGameFeaturesPlugin && Status == EGFPakLoaderStatus::Mounted && UGFPakLoaderSubsystem::GetPakLoaderSettings()->bAutoActivateGameFeatures)
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

bool UGFPakPlugin::IsValidPakPluginDirectory(const FString& InPakPluginDirectory)
{
	const FString BaseErrorMessage = GetBaseErrorMessage(TEXT("Validating"));
	
	// First, we ensure that the plugin directory exist.
	if (!FPaths::DirectoryExists(InPakPluginDirectory))
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("%s: Directory does not exist"), *BaseErrorMessage)
		return false;
	}

	// Then we ensure that we have the plugin data we need.
	PluginName = FPaths::GetBaseFilename(InPakPluginDirectory);
	
	UPluginPath = InPakPluginDirectory / PluginName + TEXT(".uplugin");
	bHasUPlugin = FPaths::FileExists(UPluginPath);
	if (!bHasUPlugin)
	{
		if (!UGFPakLoaderSubsystem::GetPakLoaderSettings()->bRequireUPluginPaks)
		{
			UPluginPath.Empty();
		}
		else
		{
			UE_LOG(LogGFPakLoader, Error, TEXT("%s: UPlugin file does not exist at location '%s'"), *BaseErrorMessage, *UPluginPath)
			return false;
		}
	}

	// ensure we have the right base directory
	const FString PluginPaksFolder = InPakPluginDirectory / PaksFolderFromDirectory;
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
	const UGFPakLoaderSubsystem* PakLoaderSubsystem = UGFPakLoaderSubsystem::Get();
	if (!ensure(IsValid(PakLoaderSubsystem)))
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("%s: The UGFPakPlugin needs to have its UGFPakLoaderSubsystem as Outer."), *BaseErrorMessage)
		Deinitialize_Internal();
		BroadcastOnStatusChange(EGFPakLoaderStatus::NotInitialized);
		return false;
	}

	// 2. We ensure the given plugin directory is a valid one and we set our variables
	if (!IsValidPakPluginDirectory(PakPluginDirectory))
	{
		Deinitialize_Internal();
		BroadcastOnStatusChange(EGFPakLoaderStatus::InvalidPluginDirectory);
		return false;
	}

	{
		const FName NewName = MakeUniqueObjectName(GetOuter(), GetClass(), FName(PluginName), EUniqueObjectNameOptions::None);
		Rename(*NewName.ToString());
	}
	
	PluginDescriptor = {};
	if (bHasUPlugin)
	{
		PluginDescriptor.Load(UPluginPath);
	}

	UE_LOG(LogGFPakLoader, Log, TEXT("Loaded the Pak Plugin data for '%s'"), *PakPluginDirectory)
	UE_LOG(LogGFPakLoader, Verbose, TEXT("  PluginName : '%s'"), *PluginName)
	UE_LOG(LogGFPakLoader, Verbose, TEXT("  UPluginPath: '%s'"), bHasUPlugin ? *UPluginPath : TEXT("- No UPlugin -"))
	UE_CLOG(bHasUPlugin, LogGFPakLoader, Verbose, TEXT("  UPlugin    : Name: '%s', Version: '%s', Description: '%s', ExplicitlyLoaded: '%s'"),
		*PluginDescriptor.FriendlyName, *PluginDescriptor.VersionName, *PluginDescriptor.Description, PluginDescriptor.bExplicitlyLoaded ? TEXT("true") : TEXT("false"))
	UE_LOG(LogGFPakLoader, Verbose, TEXT("  PluginPak  : '%s'"), *PakFilePath)

	// Now we check if there are potential issues
	if (bHasUPlugin && !PluginDescriptor.bExplicitlyLoaded)
	{
		PluginDescriptor.bExplicitlyLoaded = true;
		FText FailReason;
		if (PluginDescriptor.Save(UPluginPath, &FailReason))
		{
			UE_LOG(LogGFPakLoader, Warning, TEXT("The UPlugin descriptor file '%s' of the PakPlugin '%s' was adjusted to set ExplicitlyLoaded to true."), *UPluginPath, *PluginName)
		}
		else
		{
			UE_LOG(LogGFPakLoader, Warning, TEXT("Unable to adjust the UPlugin descriptor file '%s' of the PakPlugin '%s' to set ExplicitlyLoaded to true, the Pak Plugin might not mount. Please adjust the .uplugin manually if you encounter any error."), *UPluginPath, *PluginName)
		}
	}

	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName))
	{
		UE_LOG(LogGFPakLoader, Warning, TEXT("A Plugin with the same name as this Pak Plugin '%s' is already registered with the Plugin Manager, the Pak Plugin might not mount properly. Please remove the other plugin with the same name if you encounter any error."), *PluginName)
	}
	
	BroadcastOnStatusChange(EGFPakLoaderStatus::Unmounted);
	return true;
}

bool UGFPakPlugin::Mount_Internal()
{
	const FString BaseErrorMessage = GetBaseErrorMessage(TEXT("Mounting"));

	UGFPakLoaderSubsystem* PakLoaderSubsystem = UGFPakLoaderSubsystem::Get();
	if (!ensure(PakLoaderSubsystem) || !PakLoaderSubsystem->IsReady())
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("  %s: The PakLoaderSubsystem is not ready, unable to Mount the PakPlugin."), *BaseErrorMessage)
		Unmount_Internal();
		return false;
	}
	
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
	FGFPakLoaderPlatformFile* PakPlatformFile = PakLoaderSubsystem->GetGFPakPlatformFile(); // We need to ensure the PakPlatformFile is loaded or the following might not work
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
	UE_LOG(LogGFPakLoader, Verbose, TEXT("  Mounted the Pak Plugin '%s'"), *PakFilePath)
	
	// 4a. Now we need to register the proper MountPoint. The default MountPoint returned by the PakFile is the base folder of all the content that was packaged.
	// Depending on what was packaged in the plugin (like Engine content), the default mount point could be something like "../../../" or "../../../<project-name>/"
	// but we need the mount point to be "../../../<project-name>/Plugins[/GameFeatures]/<plugin-name>/Content/"
	// To find the proper MountPoint, we look for the AssetRegistry.bin which is located at the root of the plugin folder
	const FString OriginalMountPoint = MountedPakFile->PakGetMountPoint();
	FString MountPoint = OriginalMountPoint;
	UE_LOG(LogGFPakLoader, Verbose, TEXT("  Default Mount Point '%s'"), *MountPoint)
	
	// Here we have a different behaviour in Game and Editor because FPakPlatformFile::FindFileInPakFiles uses FPaths::MakeStandardFilename
	// The call to FPaths::MakeStandardFilename does not return the same value on Editor and Game Builds:
	// Example below with FPaths::MakeStandardFilename("../../../<project-name>/Plugins/GameFeatures/<plugin-name>/AssetRegistry.bin")
	// This is because FPlatformProcess::BaseDir() doesn't return the same value:
	// - On Editor Build, it returns the path to the Editor: "C:/Program Files/Epic Games/UE_5.3/Engine/Binaries/Win64",
	//   which is relative to RootDir "C:/Program Files/Epic Games/UE_5.3/", so MakeStandardFilename keeps the given path RELATIVE
	// - On Game Build, it returns the path to the Game Exe: "D:/.../<project-name>/Binaries/Win64/", which is not relative to RootDir, so FullPath becomes ABSOLUTE
	// For the files to be found properly, it needs to start with the MountPoint, so instead we adjust the Mount Point and adjust the filename so it does not get adjusted
	
	MountPoint = TEXT("/") + MountPoint; // adding a starting "/" stops FPaths::MakeStandardFilename from changing the path
	UE_LOG(LogGFPakLoader, Verbose, TEXT("    Setting Mount Point to '%s'"), *MountPoint)
	static_cast<FPakFile*>(MountedPakFile)->SetMountPoint(*MountPoint);
	MountPoint = MountedPakFile->PakGetMountPoint();
	UE_LOG(LogGFPakLoader, Verbose, TEXT("  Adjusted Mount Point '%s'"), *MountPoint)
	
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
		UE_LOG(LogGFPakLoader, Verbose, TEXT("  AssetRegistryPath: '%s'"), *AssetRegistryPath)
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
	UE_LOG(LogGFPakLoader, Verbose, TEXT("  bIsGameFeaturePlugin: '%s'"), bIsGameFeaturesPlugin ? TEXT("TRUE") : TEXT("FALSE"))

	// 4b. Now that we know the path of the plugin folder, we can add the main Plugin mount point if this is a Plugin DLC
	
	TSharedPtr<FPluginMountPoint> PluginContentMountPoint{};
	if (bHasUPlugin)
	{ // We add the plugin Mount Point
		const FString PluginMountPointPath = AssetRegistryFolder / TEXT("Content/");
		UE_LOG(LogGFPakLoader, Verbose, TEXT("  Adding Mount Point for Pak Plugin:  '%s' => '%s'"), *GetExpectedPluginMountPoint(), *PluginMountPointPath)
#if WITH_EDITOR
		MountPointAboutToBeMounted = {GetExpectedPluginMountPoint(), PluginMountPointPath};
#endif
		PluginContentMountPoint = PakLoaderSubsystem->AddOrCreateMountPointFromContentPath(PluginMountPointPath);
		if (PluginContentMountPoint)
		{
			PakPluginMountPoints.Add(PluginContentMountPoint);
		}
		else
		{
			UE_LOG(LogGFPakLoader, Error, TEXT("     => Unable to create the Pak Plugin Mount Point for the content folder: '%s'."), *PluginMountPointPath)
		}
#if WITH_EDITOR
		MountPointAboutToBeMounted = {};
#endif
	}
	// 4c. The assets might have been referencing content outside of their own plugin, which should have been packaged in the Pak file too. We need to create a mount point for them
	{ // Then we look at other possible MountPoints
		FPakContentFoldersFinder ContentFoldersFinder {MountPoint};
		MountedPakFile->PakVisitPrunedFilenames(ContentFoldersFinder);
		if (ContentFoldersFinder.ContentFolders.IsEmpty())
		{
			UE_LOG(LogGFPakLoader, Warning, TEXT("  %s: Unable to find any Content folder."), *BaseErrorMessage)
		}
		else
		{
			UE_LOG(LogGFPakLoader, Verbose, TEXT("  Listing all Pak Plugin Content folders:"))
			for (const FString& ContentFolder : ContentFoldersFinder.ContentFolders)
			{
				UE_LOG(LogGFPakLoader, Verbose, TEXT("   - '%s'"), *ContentFolder)
				if (!bHasUPlugin || !PluginContentMountPoint || ContentFolder != PluginContentMountPoint->GetContentPath()) // do not try to re-register the main plugin MountPoint
				{
#if WITH_EDITOR
					FString ContentPath;
					if (const TOptional<FString> MountPointName = FPluginMountPoint::GetMountPointFromContentPath(ContentFolder, &ContentPath))
					{
						MountPointAboutToBeMounted = {MountPointName.GetValue(), ContentPath};
					}
#endif
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
					UE_CLOG(PluginContentMountPoint, LogGFPakLoader, Verbose, TEXT("     => Pak Plugin Mount Point (already registered):  '%s' => '%s'"), *PluginContentMountPoint->GetRootPath(), *PluginContentMountPoint->GetContentPath())
				}
			}
#if WITH_EDITOR
			MountPointAboutToBeMounted = {};
#endif
		}
	}
	
	// 4d. As we have the asset registry, we can start loading the assets inside the Asset Registry.
	{
		IFileHandle* FileHandle = nullptr;
		{
			// The plugin needs to be temporary set as Mounted so UGFPakLoaderSubsystem::FindMountedPakContainingFile actually find the assets
			TGuardValue GuardedStatus(Status, EGFPakLoaderStatus::Mounted); 
			
			FPakGenerateFilenameMap MountedPakFilenames{OriginalMountPoint, MountPoint};
			MountedPakFile->PakVisitPrunedFilenames(MountedPakFilenames);
			PakFilenamesMap = MoveTemp(MountedPakFilenames.PakFilenamesMap);
			
			FileHandle = PakPlatformFile->OpenRead(*AssetRegistryPath);
		}
		if (FileHandle)
		{
			FArchiveFileReaderGeneric FileReader {FileHandle, *AssetRegistryPath, FileHandle->Size()};
		
			FAssetRegistryVersion::Type Version;
			FAssetRegistryLoadOptions Options(UE::AssetRegistry::ESerializationTarget::ForDevelopment);
			PluginAssetRegistry = FAssetRegistryState{};
			PluginAssetRegistry->Load(FileReader, Options, &Version);
			UE_LOG(LogGFPakLoader, Verbose, TEXT("  AssetRegistry Loaded from '%s': %d Assets in %d Packages"), *AssetRegistryPath, PluginAssetRegistry->GetNumAssets(), PluginAssetRegistry->GetNumPackages());
			
			// We make sure the newly added packages were not marked as empty. This can happen when the assets are deleted via FAssetRegistryModule::AssetDeleted
			TSet<FName> PackageNames;
			PluginAssetRegistry->EnumerateAllAssets([&PackageNames](const FAssetData& AssetData)
			{
				PackageNames.Add(AssetData.PackageName);
			});
			AddOrRemovePackagesFromAssetRegistryEmptyPackagesCache(EAddOrRemove::Remove, PackageNames);

			// Then we add them to the AssetRegistry
			IAssetRegistry& AssetRegistry = UAssetManager::Get().GetAssetRegistry();
			AssetRegistry.AppendState(*PluginAssetRegistry);
		}
		else
		{
			UE_LOG(LogGFPakLoader, Error, TEXT("  %s: Unable to load the Pak Plugin Asset Registry: '%s'."), *BaseErrorMessage, *AssetRegistryPath)
			Unmount_Internal();
			return false;
		}
	}
	
	
	// 4e. Ensure we have a valid GameFeaturesPlugin if we believe it should be one
	if (bIsGameFeaturesPlugin && ensure(PluginAssetRegistry)) //todo: test changes with GameFeatures plugin
	{
		const FAssetData* GameFeaturesData = GetGameFeatureData();
		if (!GameFeaturesData)
		{
			UE_LOG(LogGFPakLoader, Warning, TEXT("  %s: The Pak Plugin is a GameFeatures plugin but was not packaged with a UGameFeatureData asset at the root of its Content directory. The GameFeatures specific actions might not work."), *BaseErrorMessage)
			UE_LOG(LogGFPakLoader, Warning, TEXT("  bIsGameFeaturePlugin: '%s'"), bIsGameFeaturesPlugin ? TEXT("TRUE") : TEXT("FALSE"))
			bIsGameFeaturesPlugin = false;
		}
	}

	// 5. Register the plugin with the Plugin Manager
	if (bHasUPlugin)
	{
		FText FailReason;
		{
			// For UGFPakLoaderSubsystem::RegisterMountPoint to not register the wrong mount point in FPluginManager::MountPluginFromExternalSource, we need to have the plugin status to Mounted
			TOptionalGuardValue<EGFPakLoaderStatus> TemporaryStatus(Status, EGFPakLoaderStatus::Mounted);
			PluginInterface = LoadPlugin(UPluginPath, &FailReason);
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
	else
	{
		UE_LOG(LogGFPakLoader, Verbose, TEXT("  Skipping the addition of the plugin '%s' to the Plugins list as it does not have a .uplugin"), *PluginName)
	}

#if WITH_EDITOR
	// Because the content browser started to refresh some data while we were mounting but the PakPlugin was not yet ready,
	// We are asking the Content Browser to refresh
	if (UContentBrowserDataSubsystem* ContentBrowserDataSubsystem = IContentBrowserDataModule::Get().GetSubsystem())
	{
		ContentBrowserDataSubsystem->SetVirtualPathTreeNeedsRebuild();
		ContentBrowserDataSubsystem->RefreshVirtualPathTreeIfNeeded();
		ContentBrowserDataSubsystem->OnItemDataRefreshed().Broadcast(); // otherwise the changes are not picked up
	}
#endif
	
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

	if (!bHasUPlugin)
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("%s: Trying to Activate the GameFeatures of a Pak Plugin that does not have a .uplugin, which is required by the GameFeatures Subsystem"), *BaseErrorMessage)
		CompleteDelegate.ExecuteIfBound(false, {});
		return;
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
						else // If we don't have errors, we broadcast the original delegate first, and any other we might have added
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

	if (!bHasUPlugin)
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("%s: Trying to Deactivate the GameFeatures of a Pak Plugin that does not have a .uplugin, which is required by the GameFeatures Subsystem"), *BaseErrorMessage)
		CompleteDelegate.ExecuteIfBound(false, {});
		return;
	}
	
	if (Status == EGFPakLoaderStatus::DeactivatingGameFeature)
	{
		AdditionalDeactivationDelegate.Add(CompleteDelegate);
		return;
	}
	else if (Status == EGFPakLoaderStatus::Mounted)
	{
		UE_LOG(LogGFPakLoader, Log, TEXT("%s: Trying to Deactivate the GameFeatures of a Pak Plugin that is not Activated."), *BaseErrorMessage)
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
		OnDeactivatingGameFeaturesDelegate.Broadcast(this);
		
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
	//todo: double check unloading Pak Plugin Objects now that the world will be destroyed first
	
	UE_LOG(LogGFPakLoader, Verbose, TEXT("Unloading Pak Plugin Objects for the Pak Plugin '%s'..."), *PakFilePath)
	const FString BaseErrorMessage = GetBaseErrorMessage(TEXT("Unloading Pak Plugin Objects"));
	
	// We need to ensure there is no rooted package otherwise UnloadGameFeaturePlugin end up calling FGameFeaturePluginState::MarkPluginAsGarbage > UObjectBaseUtility::MarkAsGarbage which crashes if rooted
	// In our case that might happen if the current map is a world from the Pak Plugin being unloaded.
	
	// UWorld* MapWorld = GetWorld();
	// if (!ensure(MapWorld))
	// {
	// 	UE_LOG(LogGFPakLoader, Error, TEXT(" %s: the World was null"), *BaseErrorMessage)
	// 	CompleteDelegate.ExecuteIfBound(false, {});
	// 	return;
	// }
	// const UPackage* WorldPackage = MapWorld->GetPackage();
	// if (!ensure(WorldPackage))
	// {
	// 	UE_LOG(LogGFPakLoader, Error, TEXT(" %s: the World Package was null"), *BaseErrorMessage)
	// 	CompleteDelegate.ExecuteIfBound(false, {});
	// 	return;
	// }
	const UGFPakLoaderSubsystem* GFPakLoaderSubsystem = UGFPakLoaderSubsystem::Get();
	if (!ensure(GFPakLoaderSubsystem))
	{
		UE_LOG(LogGFPakLoader, Error, TEXT(" %s: the GFPakLoaderSubsystem was null"), *BaseErrorMessage)
		CompleteDelegate.ExecuteIfBound(false, {});
		return;
	}
	
	// 1. First we make sure there are no Rendering resources that might reference the objects we are about to destroy
	FlushRenderingCommands();
	// const FNameBuilder WorldPackageName(WorldPackage->GetFName());
	// const FStringView WorldPackageMountPointName = FPathViews::GetMountPointNameFromPath(WorldPackageName);
	// if (PluginName == WorldPackageMountPointName)
	// {
	// 	for (int32 LevelIndex = 0; LevelIndex < MapWorld->GetNumLevels(); LevelIndex++)
	// 	{
	// 		if (ULevel* Level = MapWorld->GetLevel(LevelIndex))
	// 		{
	// 			Level->ReleaseRenderingResources();
	// 		}
	// 	}
	// }
	
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
	// if (PluginName == WorldPackageMountPointName)
	// {
	// 	UE_LOG(LogGFPakLoader, Error, TEXT("  %s: The current World is from the GFPakPlugin being Unmounted! Reloading the default Map ..."), *BaseErrorMessage);
	// 	const UGameMapsSettings* GameMapsSettings = GetDefault<UGameMapsSettings>();
	// 	if (bIsShuttingDown)
	// 	{
	// 		UE_LOG(LogGFPakLoader, Warning, TEXT("  SHUTTING DOWN WAS REQUESTED, NOT TRYING TO LOAD A NEW WORLD"))
	// 	}
	// 	else
	// 	{
	// 		UE_LOG(LogGFPakLoader, Log, TEXT("Opening the Map '%s'"), *GameMapsSettings->GetGameDefaultMap())
	// 		UGameplayStatics::OpenLevel(this, FName(GameMapsSettings->GetGameDefaultMap()), false);
	// 		bNeedToOpenNewLevel = true;
	// 	}
	// }

	// 4. Creation of the unloading lambdas
	FString GFPluginPath = bHasUPlugin ? UGameFeaturesSubsystem::GetPluginURL_FileProtocol(UPluginPath) : "";
	auto UnloadGameFeature = [WeakThis = TWeakObjectPtr<UGFPakPlugin>(this), bNeedGameFeatureUnloading = bNeedGameFeatureUnloading, PluginName = PluginName, GFPluginPath = MoveTemp(GFPluginPath),
		BackupMountPoints = PakPluginMountPoints, bHasUPlugin = bHasUPlugin, CompleteDelegate]() mutable
	{
		UE_LOG(LogGFPakLoader, Verbose, TEXT("UnloadGameFeature"))
		UE_LOG(LogGFPakLoader, Verbose, TEXT("FinishDestroyingObjects"))
		PurgePakPluginContent(PluginName, [](const UObject* Object)
		{
			return !(Object->IsA(UGameFeatureData::StaticClass()) || Object->IsA(UWorld::StaticClass()) || Object->IsA(ULevel::StaticClass()) || Object->IsA(AWorldSettings::StaticClass()));
		});
		
		UGameFeaturesSubsystem* GFSubsystem = GEngine ? GEngine->GetEngineSubsystem<UGameFeaturesSubsystem>() : nullptr;
		if (bHasUPlugin && GFSubsystem && bNeedGameFeatureUnloading)
		{
			GFSubsystem->UnloadGameFeaturePlugin(GFPluginPath, FGameFeaturePluginLoadComplete::CreateLambda(
		[GFPluginPath, PluginName, BackupMountPoints = MoveTemp(BackupMountPoints), CompleteDelegate](const UE::GameFeatures::FResult& Result)
			{
				UE_CLOG(Result.HasError(), LogGFPakLoader, Error, TEXT("  ... Error while unloading Pak Plugin '%s':  %s"), *GFPluginPath, *Result.GetError())
				UE_CLOG(!Result.HasError(), LogGFPakLoader, Verbose, TEXT("  ... Finished unloading Pak Plugin '%s'"), *GFPluginPath)
				UE_LOG(LogGFPakLoader, Verbose, TEXT("PostUnloadGameFeature"))
				PurgePakPluginContent(PluginName, [](UObject*){ return true;});
				CompleteDelegate.ExecuteIfBound(!Result.HasError(), Result);
			}));
		}
		else
		{
			UE_LOG(LogGFPakLoader, Verbose, TEXT("PostUnloadGameFeature"))
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
			UE_LOG(LogGFPakLoader, Verbose, TEXT("PostLoadMapWithWorld"))
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
	OnUnmountingDelegate.Broadcast(this);
	
	if (PluginInterface)
	{
		FText FailReason;
		if (UnloadPlugin(PluginInterface.ToSharedRef(), &FailReason))
		{
			UE_LOG(LogGFPakLoader, Verbose, TEXT("  Pak Plugin '%s' unloaded"), *PluginName)
		}
		else
		{
			UE_LOG(LogGFPakLoader, Error, TEXT("  %s: Unable to unload the Plugin '%s':  '%s'"), *BaseErrorMessage, *PluginName, *FailReason.ToString())
		}
		PluginInterface = nullptr;
	}

	// We first remove the plugin assets from the App Asset Registry
	UnregisterPluginAssetsFromAssetRegistry();

	// Then we can delete the objects
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
			UE_LOG(LogGFPakLoader, Verbose, TEXT("  The pointer to the Pak Plugin '%s' became invalid while the plugin was Unloading its Objects"), *PakFilePath)
		}
	}));
	PakPluginMountPoints.Empty();
	
	FGFPakLoaderPlatformFile* PakPlatformFile = UGFPakLoaderSubsystem::Get() ? UGFPakLoaderSubsystem::Get()->GetGFPakPlatformFile() : nullptr; // We need to ensure the PakPlatformFile is loaded or the following might not work
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
	PakFilenamesMap.Reset();

#if WITH_EDITOR
	// We are asking the Content Browser to refresh
	MountPointAboutToBeMounted = {};
	if (UContentBrowserDataSubsystem* ContentBrowserDataSubsystem = IContentBrowserDataModule::Get().GetSubsystem())
	{
		ContentBrowserDataSubsystem->SetVirtualPathTreeNeedsRebuild();
		ContentBrowserDataSubsystem->RefreshVirtualPathTreeIfNeeded();
		ContentBrowserDataSubsystem->OnItemDataRefreshed().Broadcast(); // otherwise the changes are not picked up
	}
#endif
	
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
	OnDeinitializingDelegate.Broadcast(this);
	
	PluginName.Empty();
	PakFilePath.Empty();
	UPluginPath.Empty();
	PluginDescriptor = {};
	bHasUPlugin = false;
	bIsGameFeaturesPlugin = false;
	MountedPakFile = nullptr;
	PluginAssetRegistry.Reset();
	PakFilenamesMap.Reset();
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
			constexpr ERenameFlags PkgRenameFlags = REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional | REN_SkipGeneratedClasses;
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

	// Double-check the path matches
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
		

		if (bPluginUnmounted && !IsEngineExitRequested())
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

		UE_LOG(LogGFPakLoader, Verbose, TEXT("Unloading assets from %d plugins took %0.2f sec"), PluginNames.Num(), FPlatformTime::Seconds() - StartTime);
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

				// Remove stale entries in PackagesToUnload (unloaded world, level build data, streaming levels, external actors, etc.)
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
				if (GEditor && Obj->IsAsset())
				{
					if (UAssetEditorSubsystem* AssetEditor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
					{
						AssetEditor->CloseAllEditorsForAsset(Obj);
					}
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
		TArray<UObject*> PackagesToUnloadArray(PackagesToUnload.Array());
		for (UObject* Object : PackagesToUnload)
		{
			if (UPackage* TopLevelPackage = Object->GetPackage())
			{
				if (FLinkerLoad* LinkerToReset = FLinkerLoad::FindExistingLinkerForPackage(TopLevelPackage))
				{
				    // Calling LinkerToReset->Detach() ends up calling UObject::SetLinker which, for a UTexture2D, recreates a Resource (in UTexture2D::PostLinkerChange()) which we want to avoid
                    for (int32 ExportIndex = 0; ExportIndex < LinkerToReset->ExportMap.Num(); ++ExportIndex)
                    {
                        if (LinkerToReset->ExportMap[ExportIndex].Object)
                        {
                            LinkerToReset->ExportMap[ExportIndex].Object->ConditionalBeginDestroy();
                        }
                    }
					LinkerToReset->Detach();
				}
			}
		}

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

	//todo: is there a UE5.3 equivalent?
	// if (PackagesBeingUnloaded && ObjectsThatHadFlagsCleared.Num() > 0)
	// {
	// 	for (UPackage* PackageBeingUnloaded : *PackagesBeingUnloaded)
	// 	{
	// 		ForEachObjectWithPackage(PackageBeingUnloaded, [](UObject* Object)
	// 		{
	// 			if (ObjectsThatHadFlagsCleared.Contains(Object))
	// 			{
	// 				Object->SetFlags(RF_Standalone);
	// 			}
	// 			return true;
	// 		}, true, RF_NoFlags, UE::GC::GUnreachableObjectFlag);
	// 	}
	// }
}
#endif

void UGFPakPlugin::AddOrRemovePackagesFromAssetRegistryEmptyPackagesCache(EAddOrRemove Action, const TSet<FName>& PackageNames)
{
	if (PackageNames.IsEmpty())
	{
		return;
	}

	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		return;
	}
	
	// The AssetRegistry CachedEmptyPackages (UAssetRegistryImpl.GuardedData.CachedEmptyPackages) is only cleared in
	// UAssetRegistryImpl::AssetCreated and UAssetRegistryImpl::AssetRenamed.
	// Our problem is that when we append our DLC AssetRegistry to the main AssetRegistry, we do not want to force load the Assets to call the functions above.
	// We can get access to the cache via the DEPRECATED function IAssetRegistry::GetCachedEmptyPackages, but it is not thread safe.
	// The following 2 functions allow us to pass a callback while a lock has been created:
	// UAssetRegistryImpl::ReadLockEnumerateTagToAssetDatas and UAssetRegistryImpl::EnumerateAllPackages
	// so we use them to clear the cache.
	// todo: check with Epic if AssetRegistry::AppendState could clear the empty packages, or expose a way to add/remove them
	
	// This lambda to remove the given packages from the cache is not threadsafe
	auto RemovePackagesFromCache = [&PackageNames, &AssetRegistry, Action]()
	{
		const TSet<FName>& CachedEmptyPackagesConst = AssetRegistry->GetCachedEmptyPackages(); // using deprecated function! But haven't found an easy alternative
		TSet<FName>& CachedEmptyPackages = const_cast<TSet<FName>&>(CachedEmptyPackagesConst); // remove const-ness from the cache
		if (Action == EAddOrRemove::Add)
		{
			for (FName PackageName : PackageNames)
			{
				CachedEmptyPackages.Add(PackageName);
			}
		}
		else
		{
			for (FName PackageName : PackageNames)
			{
				CachedEmptyPackages.Remove(PackageName);
			}
		}
		
	};
	
	bool bHasUpdated = false;

	// As both ReadLockEnumerateTagToAssetDatas and EnumerateAllPackages don't have a way to short circuit to for loop (we don't care about the assets, we just want the lock)
	// we try calling first the function which should have the lesser number of items
	AssetRegistry->ReadLockEnumerateTagToAssetDatas([&bHasUpdated, &RemovePackagesFromCache](FName TagName, const TArray<const FAssetData*>& Assets)
	{
		if (!bHasUpdated)
		{
			RemovePackagesFromCache();
			bHasUpdated = true;
		}
	});

	if (bHasUpdated)
	{
		return;
	}
	
	AssetRegistry->ReadLockEnumerateTagToAssetDatas([&bHasUpdated, &RemovePackagesFromCache](FName TagName, const TArray<const FAssetData*>& Assets)
	{
		if (!bHasUpdated)
		{
			RemovePackagesFromCache();
			bHasUpdated = true;
		}
	});
}

void UGFPakPlugin::UnregisterPluginAssetsFromAssetRegistry()
{
	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		return;
	}

	UE_LOG(LogGFPakLoader, Log, TEXT("Removing Assets from the Asset Registry"))
	FlushAsyncLoading(); // to be sure we don't have assets to be deleted that are pending load

	TSet<FName> PackagePathsToRemove;
	TSet<FName> PackageNamesToRemove;
	PluginAssetRegistry->EnumerateAllAssets([AssetRegistry, &PackagePathsToRemove, &PackageNamesToRemove](const FAssetData& AssetData)
	{
		UE_CLOG(AssetData.AssetClassPath == UWorld::StaticClass()->GetClassPathName(), LogGFPakLoader, Verbose, TEXT("Removing MAP Asset from Asset Registry: '%s'"), *AssetData.GetObjectPathString())
		UE_CLOG(AssetData.AssetClassPath != UWorld::StaticClass()->GetClassPathName(), LogGFPakLoader, VeryVerbose, TEXT("   Removing Asset from Asset Registry: '%s'"), *AssetData.GetObjectPathString())
		PackagePathsToRemove.Add(AssetData.PackagePath);
		
		UPackage* DeletedObjectPackage = AssetData.PackageName.IsNone() ? nullptr : FindPackage(nullptr, *AssetData.PackageName.ToString()); // do not force load the package
		UObject* AssetToDelete = AssetData.FastGetAsset();
		const bool bIsEmptyPackage = !IsValid(DeletedObjectPackage) || UPackage::IsEmptyPackage(DeletedObjectPackage, AssetToDelete);

		// The below is the Runtime and Editor equivalent of ObjectTools::DeleteSingleObject(AssetToDelete, false) which is editor only

		if (AssetToDelete)
		{
#if WITH_EDITOR
			if (GEditor)
			{
				if (GEditor->GetSelectedObjects())
				{
					GEditor->GetSelectedObjects()->Deselect(AssetToDelete);
				}
				if (AssetToDelete->IsAsset())
				{
					if (UAssetEditorSubsystem* AssetEditor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
					{
						AssetEditor->CloseAllEditorsForAsset(AssetToDelete);
					}
				}
			}
#endif

			if (UWorld* World = Cast<UWorld>(AssetToDelete))
			{
				World->CleanupWorld();
			}
		}
		
		if (bIsEmptyPackage)
		{
			PackageNamesToRemove.Add(AssetData.PackageName);
#if WITH_EDITOR
			// If there is a package metadata object, clear the standalone flag so the package can be truly emptied upon GC
			if (UMetaData* MetaData = DeletedObjectPackage ? DeletedObjectPackage->GetMetaData() : nullptr)
			{
				MetaData->ClearFlags(RF_Standalone);
			}
#endif
		}
		
		if (AssetToDelete)
		{
			AssetToDelete->MarkPackageDirty();

			// Equivalent of FAssetRegistryModule::AssetDeleted( AssetToDelete ) which is Editor only
			{
				// We might need to find a way to add the package to the EmptyPackageCache as per FAssetRegistryModule::AssetDeleted, but it works without

				// Let subscribers know that the asset was removed from the registry
				AssetRegistry->OnAssetRemoved().Broadcast(AssetData);
				AssetRegistry->OnAssetsRemoved().Broadcast({AssetData});

				// Notify listeners that an in-memory asset was just deleted
				AssetRegistry->OnInMemoryAssetDeleted().Broadcast(AssetToDelete);
			}
			// Remove standalone flag so garbage collection can delete the object and public flag so that the object is no longer considered to be an asset
			AssetToDelete->ClearFlags(RF_Standalone | RF_Public);
		}
		else
		{
			AssetRegistry->OnAssetRemoved().Broadcast(AssetData);
			AssetRegistry->OnAssetsRemoved().Broadcast({AssetData});
		}

#if WITH_EDITOR
		if (bIsEmptyPackage && DeletedObjectPackage)
		{
			AssetRegistry->PackageDeleted(DeletedObjectPackage);
		}
#endif
	});

	AddOrRemovePackagesFromAssetRegistryEmptyPackagesCache(EAddOrRemove::Add, PackageNamesToRemove);
	
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	FlushRenderingCommands();

	{
		TSet<FString> FilenamesToRemove;
		for (const TTuple<FName, TSharedPtr<const FGFPakFilenameMap>>& Entry : PakFilenamesMap)
		{
			if (Entry.Value && !Entry.Value->MountedPackageName.IsNone())
			{
				FilenamesToRemove.Add(Entry.Value->ProjectAdjustedFullFilename);
			}
		}
		const TArray<FString> Filenames = FilenamesToRemove.Array();
		AssetRegistry->ScanModifiedAssetFiles(Filenames);
	}
	// ScanModifiedAssetFiles might remove assets from the cache data, so we need to remove paths after
	for (FName& PackagePath : PackagePathsToRemove)
	{
		const bool bResult = AssetRegistry->RemovePath(PackagePath.ToString());
		UE_CLOG(bResult, LogGFPakLoader, Verbose, TEXT(" - Removed the Path '%s'"), *PackagePath.ToString())
		UE_CLOG(!bResult, LogGFPakLoader, Verbose, TEXT(" - ! Unable to remove the Path '%s' !"), *PackagePath.ToString())
	}
}

#undef LOCTEXT_NAMESPACE
