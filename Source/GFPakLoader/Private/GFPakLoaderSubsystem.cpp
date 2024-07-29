// Copyright GeoTech BV


#include "GFPakLoaderSubsystem.h"

#include "GFPakLoaderLog.h"
#include "GFPakLoaderPlatformFile.h"
#include "GFPakLoaderSettings.h"
#include "Algo/Find.h"
#include "Engine/AssetManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "UObject/AssetRegistryTagsContext.h"

#if WITH_EDITOR
#include "Selection.h"
#endif

class FDirectoryLister : public IPlatformFile::FDirectoryVisitor
{
public:
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		if (bIsDirectory)
		{
			Directories.Add(FilenameOrDirectory);
		}
		return true;
	}
	TArray<FString> Directories;
};



void UGFPakLoaderSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UE_LOG(LogGFPakLoader, Verbose, TEXT("Initializing the UGFPakLoaderSubsystem..."))
	// Collection.InitializeDependency(UGameFeaturesSubsystem::StaticClass());
	bIsShuttingDown = false;
	
	Super::Initialize(Collection);
	
	TWeakObjectPtr<UGFPakLoaderSubsystem> WeakSubsystem{this};
	FDelayedAutoRegisterHelper(EDelayedRegisterRunPhase::EndOfEngineInit,
		[WeakSubsystem]()
		{
			if (UGFPakLoaderSubsystem* Subsystem = WeakSubsystem.Get())
			{
				Subsystem->OnEngineLoopInitCompleted();
			}
		});

	UAssetManager::CallOrRegister_OnAssetManagerCreated(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &UGFPakLoaderSubsystem::OnAssetManagerCreated));
}

void UGFPakLoaderSubsystem::Deinitialize()
{
	bIsShuttingDown = true;
	UE_LOG(LogGFPakLoader, Verbose, TEXT("Deinitializing the UGFPakLoaderSubsystem..."))
	OnSubsystemShuttingDownDelegate.Broadcast();

	// todo: there might still be some multithreading issues as we are deinitializing the PakPlugins outside of the GameFeaturesPakPluginsLock Write lock (which is needed to avoid deadlocks)
	// Each PakPlugin should have its own lock to access its content as it can be accessed from the AsyncLoadingThread via FGFPakLoaderPlatformFile
	EnumeratePakPlugins([](UGFPakPlugin* PakPlugin)
	{
		PakPlugin->Deinitialize();
		PakPlugin->ConditionalBeginDestroy();
		return EForEachResult::Continue;
	});
	
	{
		FRWScopeLock Lock(GameFeaturesPakPluginsLock, SLT_Write);
		GameFeaturesPakPlugins.Empty();
	}
	
	// We restore the original delegate. See UGFPakLoaderSubsystem::Initialize for more explanations
	IPluginManager::Get().SetRegisterMountPointDelegate(IPluginManager::FRegisterMountPointDelegate::CreateStatic(&FPackageName::RegisterMountPoint));
	
	FPackageName::OnContentPathMounted().RemoveAll(this);
	FPackageName::OnContentPathDismounted().RemoveAll(this);

	FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	
	bAssetManagerCreated = false;
	bStarted = false;
	UE_LOG(LogGFPakLoader, Verbose, TEXT("...Deinitialized the UGFPakLoaderSubsystem"))
	OnSubsystemShutdownDelegate.Broadcast();
}

FString UGFPakLoaderSubsystem::GetDefaultPakPluginFolder() const
{
	return GetPakLoaderSettings()->GetAbsolutePakLoadPath();
}

void UGFPakLoaderSubsystem::AddPakPluginFolder(const FString& InPakPluginFolder, TArray<UGFPakPlugin*>& NewlyAddedPlugins, TArray<UGFPakPlugin*>& AllPluginsInFolder)
{
	NewlyAddedPlugins.Empty();
	AllPluginsInFolder.Empty();

	const FString PakPluginFolderOriginal = InPakPluginFolder.IsEmpty() ? GetDefaultPakPluginFolder() : InPakPluginFolder; 
	const FString PakPluginFolder = FPaths::ConvertRelativePathToFull(PakPluginFolderOriginal); // Converts relative to FPlatformProcess::BaseDir()
	if (!IsReady())
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("UGFPakLoaderSubsystem is not ready. Unable to AddPakPluginFolder '%s'"), *PakPluginFolder)
		return;
	}
	if (!FPaths::DirectoryExists(PakPluginFolder))
	{
		if (!InvalidDirectories.Contains(PakPluginFolder))
		{
			UE_LOG(LogGFPakLoader, Error, TEXT("Unable to load the Pak Plugin folder because the directory does not exist: '%s' (Folder requested: '%s')"), *PakPluginFolder, *PakPluginFolderOriginal)
			InvalidDirectories.Add(PakPluginFolder);
		}
		return;
	}
	InvalidDirectories.Remove(PakPluginFolder);
	
	UE_LOG(LogGFPakLoader, VeryVerbose, TEXT("Adding Pak Plugins located in the folder '%s'..."), *PakPluginFolder)
	
	IFileManager& FileManager = IFileManager::Get();
	FDirectoryLister DirectoryLister;
	FileManager.IterateDirectory(*PakPluginFolder, DirectoryLister);

	int NbFailed = 0;
	for (const FString& PluginPath : DirectoryLister.Directories)
	{
		// First, we ignore DLC folders starting with the IgnorePakWithPrefix, useful for testing
		const FString PluginFolderName = FPaths::GetBaseFilename(PluginPath);
		if (!GetPakLoaderSettings()->IgnorePakWithPrefix.IsEmpty() && PluginFolderName.StartsWith(GetPakLoaderSettings()->IgnorePakWithPrefix))
		{
			if (!IgnoredPluginPaths.Contains(PluginPath))
			{
				UE_LOG(LogGFPakLoader, Warning, TEXT("Ignoring the potential plugin located at '%s' because its folder name '%s' starts with '%s', as per the IgnorePakWithPrefix of the Pak Loader Settings"),
				*FPaths::ConvertRelativePathToFull(PluginPath), *PluginFolderName, *GetPakLoaderSettings()->IgnorePakWithPrefix)
			}
			IgnoredPluginPaths.Add(PluginPath);
			continue;
		}
		
		bool bIsNewlyAdded = false;
		if (UGFPakPlugin* Plugin = GetOrAddPakPlugin(PluginPath, bIsNewlyAdded))
		{
			if (bIsNewlyAdded)
			{
				NewlyAddedPlugins.Add(Plugin);
			}
			AllPluginsInFolder.Add(Plugin);
		}
		else
		{
			++NbFailed;
		}
	}

	const int NbAddedPlugins = NewlyAddedPlugins.Num();
	const int NbExistingPlugins = AllPluginsInFolder.Num() - NbAddedPlugins;
	UE_CLOG(NbAddedPlugins > 0, LogGFPakLoader, Log, TEXT("Adding Pak Plugins located in the folder '%s': %d Pak Plugins were added, %d Plugins were already referenced by the subsystem, and %d folders were not valid Pak Plugin folders"),
		*PakPluginFolder, NbAddedPlugins, NbExistingPlugins, NbFailed)
	UE_CLOG(NbAddedPlugins == 0, LogGFPakLoader, VeryVerbose, TEXT("... %d Pak Plugins were added, %d Plugins were already referenced by the subsystem, and %d folders were not valid Pak Plugin folders"),
		NbAddedPlugins, NbExistingPlugins, NbFailed)
}

UGFPakPlugin* UGFPakLoaderSubsystem::GetOrAddPakPlugin(const FString& InPakPluginPath, bool& bIsNewlyAdded)
{
	FString PakPluginPath = FPaths::ConvertRelativePathToFull(InPakPluginPath);
	if (!IsReady())
	{
		bIsNewlyAdded = false;
		UE_LOG(LogGFPakLoader, Error, TEXT("UGFPakLoaderSubsystem is not ready. Unable to AddPakPlugin '%s'"), *PakPluginPath)
		return nullptr;
	}
	
	UGFPakPlugin* PakPlugin;
	{
		FRWScopeLock Lock(GameFeaturesPakPluginsLock, SLT_ReadOnly);
		UGFPakPlugin** ExistingPakPlugin = Algo::FindByPredicate(GameFeaturesPakPlugins,
			[&PakPluginPath](const UGFPakPlugin* PakPlugin)
			{
				return FPaths::IsSamePath(PakPlugin->GetPakPluginDirectory(), PakPluginPath);
			});

		bIsNewlyAdded = !ExistingPakPlugin;
		if (!bIsNewlyAdded)
		{
			return *ExistingPakPlugin;
		}
	}
	{
		FRWScopeLock Lock(GameFeaturesPakPluginsLock, SLT_Write);
		
		PakPlugin = NewObject<UGFPakPlugin>(this);
		PakPlugin->SetPakPluginDirectory(PakPluginPath);
	
		GameFeaturesPakPlugins.Emplace(PakPlugin); // need to be done here as the UGFPakLoaderSubsystem::RegisterMountPoint might get triggered on loading
	}
	
	PakPlugin->OnStatusChanged().AddDynamic(this, &ThisClass::PakPluginStatusChanged);
	PakPlugin->OnDestroyed().AddDynamic(this, &ThisClass::PakPluginDestroyed);
	
	
	OnPakPluginAddedDelegate.Broadcast(PakPlugin);
	
	if (PakPlugin->LoadPluginData())
	{
		return PakPlugin;
	}
	
	PakPlugin->ConditionalBeginDestroy();
	return nullptr;
}

TSharedPtr<FPluginMountPoint> UGFPakLoaderSubsystem::AddOrCreateMountPointFromContentPath(const FString& InContentPath)
{
	if (!IsReady())
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("UGFPakLoaderSubsystem is not ready. Unable to AddOrCreateMountPointFromContentPath '%s'"), *InContentPath)
		return {};
	}
	
	const TWeakPtr<FPluginMountPoint>* MountPointPtr = MountPoints.Find(InContentPath);
	if (MountPointPtr && MountPointPtr->IsValid())
	{
		TSharedPtr<FPluginMountPoint> MountPoint = MountPointPtr->Pin();
		if (ensure(MountPoint))
		{
			return MountPoint;
		}
	}

	TOptional<FPluginMountPoint> ContentMountPoint = FPluginMountPoint::CreateFromContentPath(InContentPath);
	if (ContentMountPoint)
	{
		UE_LOG(LogGFPakLoader, Verbose, TEXT("     => Added %sMountPoint:  '%s' => '%s'"),
				ContentMountPoint->NeedsUnregistering() ? TEXT("") : TEXT("Existing "), *ContentMountPoint->GetRootPath(), *ContentMountPoint->GetContentPath())
		
		// Here we create a shared Pointer with a custom Deleter as we statically keep track of the MountPoints.
		TSharedPtr<FPluginMountPoint> MountPoint(new FPluginMountPoint(MoveTemp(ContentMountPoint.GetValue())),
			[InContentPath, WeakSubsystem = TWeakObjectPtr<UGFPakLoaderSubsystem>(this)](FPluginMountPoint* DeletedMountPoint)
		{
			if (ensure(DeletedMountPoint))
			{
				MountPoints.Remove(DeletedMountPoint->GetContentPath());
				UE_LOG(LogGFPakLoader, Verbose, TEXT("Deleted FPluginMountPoint from Subsystem:  '%s' => '%s'"), *DeletedMountPoint->GetRootPath(), *DeletedMountPoint->GetContentPath())
				UE_LOG(LogGFPakLoader, Verbose, TEXT("UGFPakLoaderSubsystem::MountPoints.Remove():  %d Mount Points"), MountPoints.Num())
				if (DeletedMountPoint->NeedsUnregistering())
				{
					if (UGFPakLoaderSubsystem* Subsystem = WeakSubsystem.Get())
					{
						if (FGFPakLoaderPlatformFile* PakPlatformFile = static_cast<FGFPakLoaderPlatformFile*>(FPlatformFileManager::Get().FindPlatformFile(FGFPakLoaderPlatformFile::GetTypeName())))
						{
							PakPlatformFile->UnregisterPakContentPath(DeletedMountPoint->GetContentPath());
						}
					}
				}
				
				SharedPointerInternals::DefaultDeleter<FPluginMountPoint> DefaultDeleter{};
				DefaultDeleter(DeletedMountPoint);
			}
			else
			{
				UE_LOG(LogGFPakLoader, Error, TEXT("Error while deleting FPluginMountPoint for '%s' from Subsystem:  FPluginMountPoint is null"), *InContentPath)
				UE_LOG(LogGFPakLoader, Verbose, TEXT("UGFPakLoaderSubsystem::MountPoints:  %d Mount Points pre maintenance"), MountPoints.Num())
				for (auto Iterator = MountPoints.CreateIterator(); Iterator; ++Iterator)
				{
					if (!Iterator->Value.IsValid())
					{
						Iterator.RemoveCurrent();
					}
				}
				UE_LOG(LogGFPakLoader, Verbose, TEXT("UGFPakLoaderSubsystem::MountPoints:  %d Mount Points post maintenance"), MountPoints.Num())
			}
		});

		if (MountPoint->NeedsUnregistering())
		{
			if (FGFPakLoaderPlatformFile* PakPlatformFile = GetGFPakPlatformFile())
			{
				PakPlatformFile->RegisterPakContentPath(MountPoint->GetContentPath());
			}
		}
		
		MountPoints.Add(InContentPath, MountPoint->AsWeak());
		UE_LOG(LogGFPakLoader, Verbose, TEXT("UGFPakLoaderSubsystem::MountPoints.Add():  %d Mount Points"), MountPoints.Num())
		return MountPoint;
	}
	else
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("     => Unable to create the Mount Point for the content folder: '%s'."), *InContentPath)
	}
	return {};
}

FGFPakLoaderPlatformFile* UGFPakLoaderSubsystem::GetGFPakPlatformFile()
{
	if (!GFPakPlatformFile)
	{
		GFPakPlatformFile = static_cast<FGFPakLoaderPlatformFile*>(FPlatformFileManager::Get().FindPlatformFile(FGFPakLoaderPlatformFile::GetTypeName()));
		if (!GFPakPlatformFile)
		{
			// Create a pak platform file and mount the feature pack file.
			GFPakPlatformFile = static_cast<FGFPakLoaderPlatformFile*>(FPlatformFileManager::Get().GetPlatformFile(FGFPakLoaderPlatformFile::GetTypeName()));
			GFPakPlatformFile->Initialize(&FPlatformFileManager::Get().GetPlatformFile(), TEXT("")); // TEXT("-UseIoStore")
			GFPakPlatformFile->InitializeNewAsyncIO(); // needed in Game builds to ensure PakPrecacherSingleton is valid
			FPlatformFileManager::Get().SetPlatformFile(*GFPakPlatformFile); // We want this to be the main one, as it will dispatch the calls to the right PlatformFile
		}
	}
	return GFPakPlatformFile;
}

UGFPakPlugin* UGFPakLoaderSubsystem::FindMountedPakContainingFile(const TCHAR* OriginalFilename, FString* PakAdjustedFilename)
{
	const FName FilenameF {OriginalFilename};
	const FString Extension {FPaths::GetExtension(OriginalFilename, true)};
	
	UGFPakPlugin* Plugin = nullptr;
	EnumeratePakPluginsWithStatus<EComparison::GreaterOrEqual>(EGFPakLoaderStatus::Mounted, [&Plugin, &FilenameF, &PakAdjustedFilename](UGFPakPlugin* PakPlugin)
	{
		if (const TSharedPtr<const FGFPakFilenameMap>* MountedFilenameFound = PakPlugin->GetPakFilenamesMap().Find(FilenameF))
		{
			if (const TSharedPtr<const FGFPakFilenameMap>& MountedFilename = *MountedFilenameFound)
			{
				Plugin = PakPlugin;
				if (PakAdjustedFilename)
				{
					*PakAdjustedFilename = MountedFilename->AdjustedFullFilename;
				}
				return EForEachResult::Break;
			}
		}
		return EForEachResult::Continue;
	});
	
	if (!Plugin && PakAdjustedFilename)
	{
		PakAdjustedFilename->Empty();
	}
	return Plugin;
}

void UGFPakLoaderSubsystem::OnAssetManagerCreated()
{
	bAssetManagerCreated = true;
	UE_LOG(LogGFPakLoader, Verbose, TEXT(" UAssetManager::CallOrRegister_OnAssetManagerCreated called"))
	Start();
}

void UGFPakLoaderSubsystem::OnEngineLoopInitCompleted()
{
	bEngineLoopInitCompleted = true;
	UE_LOG(LogGFPakLoader, Verbose, TEXT(" FDelayedAutoRegisterHelper(EDelayedRegisterRunPhase::EndOfEngineInit) called"))
	Start();
}

void UGFPakLoaderSubsystem::PreLoadMapWithContext(const FWorldContext& WorldContext, const FString& MapName)
{
	UWorld* NewWorld = nullptr;

	// Inspired by UEngine::LoadMap, but handling the case where a Package is valid but the its World has already been GCed
	UPackage* WorldPackage = FindPackage(nullptr, *MapName);

	if (WorldPackage)
	{
		NewWorld = UWorld::FindWorldInPackage(WorldPackage);

		// If the world was not found, it could be a redirector to a world. If so, follow it to the destination world.
		if (!NewWorld)
		{
			NewWorld = UWorld::FollowWorldRedirectorInPackage(WorldPackage);
		}
	}

	if (!NewWorld)
	{
		// Here trying to match (WorldContext.WorldType == EWorldType::PIE ? LOAD_PackageForPIE : LOAD_None)
		const ELoadFlags LoadFlags = WorldPackage && WorldPackage->HasAnyPackageFlags(PKG_PlayInEditor) ? LOAD_PackageForPIE : LOAD_None;
		WorldPackage = LoadPackage(nullptr, *MapName, LoadFlags);

		NewWorld = UWorld::FindWorldInPackage(WorldPackage);

		if (!NewWorld)
		{
			NewWorld = UWorld::FollowWorldRedirectorInPackage(WorldPackage);
		}
	}

	if (IsValid(NewWorld))
	{
		NewWorld->AddToRoot(); // Root Flag will be removed when the Map is unloaded
	}
}

void UGFPakLoaderSubsystem::Start()
{
	if (!IsReady() || bStarted)
	{
		return;
	}
	bStarted = true;
	
	Debug_LogPaths();
	
	UE_LOG(LogGFPakLoader, Verbose, TEXT(" UGFPakLoaderSubsystem Starting..."))

#if WITH_EDITOR
	if (LogGFPakLoader.GetVerbosity() <= ELogVerbosity::Verbose)
	{
		FPackageName::OnContentPathMounted().AddUObject(this, &UGFPakLoaderSubsystem::OnContentPathMounted);
		FPackageName::OnContentPathDismounted().AddUObject(this, &UGFPakLoaderSubsystem::OnContentPathDismounted);
	}
#endif
	
	// The GameFeatureSubsystem technically handles the registering and the mounting of the Plugin with the IPluginManager, but by doing so, it ends up
	// creating a new Mount point for our assets to be mounted.
	// Our packaged assets do not exist at a specific location on disk, so we need the Mount Point to match their path in the Pak file which is relative:
	//   => '../../../<project-name>/Plugins/GameFeatures/GFPlugin/Content/' mapped as  '/GFPlugin/'
	// The problem is that if we let the PluginManager create the MountPoint, it will end up being an absolute path and the assets will not resolve:
	//   => 'D:/path/<project-name>/<DLC>/GFPlugin/Content/' mounted to '/GFPlugin/'
	// Because the Mount Point would be created everytime the GameFeatures is activated, we need to ensure it is not created in the first place.
	// As per the IPluginManager and GameFeatureSubsystem code, setting the plugin to not be ExplicitlyLoaded will stop it from automatically creating a Mount Point, but will make other parts not work properly.
	// Instead, we override the IPluginManager::RegisterMountPointDelegate with our own where we stop the creation of a mount point for one of our Pak Plugin
	// and we restore the regular delegate when the subsystem is destroyed.
	IPluginManager::Get().SetRegisterMountPointDelegate(IPluginManager::FRegisterMountPointDelegate::CreateUObject(this, &UGFPakLoaderSubsystem::RegisterMountPoint));
	UE_LOG(LogGFPakLoader, Verbose, TEXT("...Initialized the UGFPakLoaderSubsystem"))
	
	OnSubsystemReadyDelegate.Broadcast();
	
	if (GetPakLoaderSettings()->bAddPakPluginsFromStartupLoadDirectory)
	{
		const FString Path = GetDefaultPakPluginFolder();
		if (FPaths::DirectoryExists(Path))
		{
			UE_LOG(LogGFPakLoader, Log, TEXT("Adding Pak Plugin from default folder '%s'"), *Path)
			TArray<UGFPakPlugin*> NewPlugins;
			TArray<UGFPakPlugin*> AllPlugins;
			AddPakPluginFolder(Path, NewPlugins, AllPlugins);
		}
		else
		{
			UE_LOG(LogGFPakLoader, Warning, TEXT("The default Pak folder location from the GF Pak Loader Settings does not exist: '%s'"), *Path)
		}
	}
	
	OnEnsureWorldIsLoadedInMemoryBeforeLoadingMapChanged();
	
	OnStartupPaksAddedDelegate.Broadcast();
}

void UGFPakLoaderSubsystem::PakPluginStatusChanged(UGFPakPlugin* PakPlugin, EGFPakLoaderStatus OldValue, EGFPakLoaderStatus NewValue)
{
	OnPakPluginStatusChangedDelegate.Broadcast(PakPlugin, OldValue, NewValue);
}

void UGFPakLoaderSubsystem::PakPluginDestroyed(UGFPakPlugin* PakPlugin)
{
	OnPakPluginRemovedDelegate.Broadcast(PakPlugin);
	if (!bIsShuttingDown)
	{
		FRWScopeLock Lock(GameFeaturesPakPluginsLock, SLT_Write);
		GameFeaturesPakPlugins.Remove(PakPlugin);
	}
	
	if (PakPlugin)
	{
		PakPlugin->OnStatusChanged().RemoveAll(this);
	}
}

void UGFPakLoaderSubsystem::RegisterMountPoint(const FString& RootPath, const FString& ContentPath)
{
	// To double-check the Mount Points being added, we could listen to the delegate FPackageName::OnContentPathMounted()
	{
		FRWScopeLock Lock(GameFeaturesPakPluginsLock, SLT_ReadOnly);
		for (const UGFPakPlugin* GFPakPlugin : GameFeaturesPakPlugins)
		{
			if (ensure(IsValid(GFPakPlugin)) && GFPakPlugin->GetStatus() > EGFPakLoaderStatus::Unmounted)
			{
				if (RootPath == GFPakPlugin->GetExpectedPluginMountPoint() && FPackageName::MountPointExists(RootPath))
				{
					UE_LOG(LogGFPakLoader, Warning, TEXT("UGFPakLoaderSubsystem stopped the IPluginManager from registering the PakPlugin MountPoint  '%s' => '%s'"), *RootPath, *ContentPath)
					return;
				}
			}
		}
	}
	
	UE_LOG(LogGFPakLoader, Log, TEXT("UGFPakLoaderSubsystem::RegisterMountPoint: About to register MountPoint  '%s' => '%s'"), *RootPath, *ContentPath)
	FPackageName::RegisterMountPoint(RootPath, ContentPath);
}

void UGFPakLoaderSubsystem::Debug_LogPaths()
{
	UE_LOG(LogGFPakLoader, Verbose, TEXT(" === UGFPakLoaderSubsystem: FPaths Functions IN '%s' ==="), GIsEditor ? TEXT("EDITOR") : TEXT("GAME") );
	FString BaseDir = FPlatformProcess::BaseDir();
	FString RootDir = FPlatformMisc::RootDir();
	FString ProjectDir = FPlatformMisc::ProjectDir();
	UE_LOG(LogGFPakLoader, Verbose, TEXT("  BaseDir         '%s' "), *BaseDir);
	UE_LOG(LogGFPakLoader, Verbose, TEXT("  RootDir         '%s' "), *RootDir);
	UE_LOG(LogGFPakLoader, Verbose, TEXT("  ProjectDir      '%s' "), *ProjectDir);
	UE_LOG(LogGFPakLoader, Verbose, TEXT("  Full ProjectDir '%s' "), *FPaths::ConvertRelativePathToFull(ProjectDir));
		
	TArray<FString> FilenamesToTest;
	FilenamesToTest.Add(TEXT("/../../../Project/Plugins/GameFeatures/plugin-name/AssetRegistry.bin"));
	FilenamesToTest.Add(TEXT("../../../Project/Plugins/GameFeatures/plugin-name/AssetRegistry.bin"));
	FilenamesToTest.Add(TEXT("/../../../../"));
	FilenamesToTest.Add(TEXT("../../../../"));
	FilenamesToTest.Add(TEXT("/../../../"));
	FilenamesToTest.Add(TEXT("../../../"));
	FilenamesToTest.Add(TEXT("../../"));
	FilenamesToTest.Add(TEXT("../"));
	FilenamesToTest.Add(TEXT("./"));
	FilenamesToTest.Add(TEXT("/"));

	for (FString& FilenameTest : FilenamesToTest)
	{
		FString StandardFilenameTest = FilenameTest;
		FPaths::MakeStandardFilename(StandardFilenameTest);
			
		FString Standardized = FPaths::ConvertRelativePathToFull(StandardFilenameTest);
		// remove duplicate slashes
		FPaths::RemoveDuplicateSlashes(Standardized);
			
		UE_LOG(LogGFPakLoader, Verbose, TEXT("  - Test for Path ----------------    '%s'"), *FilenameTest);
		UE_LOG(LogGFPakLoader, Verbose, TEXT("      => MakeStandardFilename         '%s'"), *StandardFilenameTest);
		UE_LOG(LogGFPakLoader, Verbose, TEXT("      => ConvertRelativePathToFull    '%s'"), *Standardized);
	}
}

void UGFPakLoaderSubsystem::OnContentPathMounted(const FString& AssetPath, const FString& ContentPath)
{
	UE_LOG(LogGFPakLoader, Verbose, TEXT("OnContentPathMounted:     '%s'  =>  '%s'"), *AssetPath, *ContentPath);
}

void UGFPakLoaderSubsystem::OnContentPathDismounted(const FString& AssetPath, const FString& ContentPath)
{
	UE_LOG(LogGFPakLoader, Verbose, TEXT("OnContentPathDismounted:  '%s'  =>  '%s'"), *AssetPath, *ContentPath);
}

void UGFPakLoaderSubsystem::OnEnsureWorldIsLoadedInMemoryBeforeLoadingMapChanged()
{
	if (GetPakLoaderSettings()->bEnsureWorldIsLoadedInMemoryBeforeLoadingMap)
	{
		if (!FCoreUObjectDelegates::PreLoadMapWithContext.IsBoundToObject(this))
		{
			FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &UGFPakLoaderSubsystem::PreLoadMapWithContext);
		}
	}
	else
	{
		FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	}
}

void UGFPakLoaderSubsystem::OnPreAddPluginAssetRegistry(const FAssetRegistryState& PluginAssetRegistry, UGFPakPlugin* Plugin)
{
	IAssetRegistry& AssetRegistry = UAssetManager::Get().GetAssetRegistry();
	
	FScopeLock AssetOwnerLock(&AssetOwnerMutex);
	PluginAssetRegistry.EnumerateAllAssets([this, &AssetRegistry, Plugin](const FAssetData& AssetData)
	{
		FSoftObjectPath AssetPath = AssetData.GetSoftObjectPath();
		FAssetOwner* AssetOwner = AssetOwners.Find(AssetPath);
		if (!AssetOwner)
		{
			AssetOwner = &AssetOwners.Add(AssetPath);
			
			const FAssetData GameAssetData = AssetRegistry.GetAssetByObjectPath(AssetPath, false, false);
			if (GameAssetData.IsValid())
			{
				AssetOwner->bIsBaseGameAsset = true;
				AssetOwner->BaseGameAssetData = GameAssetData;
			}
		}
		AssetOwner->PluginOwners.AddUnique(Plugin);
	});
}

void UGFPakLoaderSubsystem::OnPreRemovePluginAssetRegistry(const FAssetRegistryState& PluginAssetRegistry, UGFPakPlugin* Plugin, TSet<FName>& OutPackageNamesToRemove)
{
	IAssetRegistry* AssetRegistryPtr = IAssetRegistry::Get();
	if (!AssetRegistryPtr)
	{
		return;
	}
	
	FScopeLock AssetOwnerLock(&AssetOwnerMutex);
	TSet<FSoftObjectPath> AlreadyRemovedAssets;
	PluginAssetRegistry.EnumerateAllAssets([this, Plugin, AssetRegistryPtr, &OutPackageNamesToRemove, &AlreadyRemovedAssets]
		(const FAssetData& AssetData)
	{
		FSoftObjectPath AssetPath = AssetData.GetSoftObjectPath();
		bool bRemoveAsset = true;
		if (FAssetOwner* AssetOwner = AssetOwners.Find(AssetPath)) // If new, check if the given asset is part of the Base Game Content
		{
			bRemoveAsset = false;
			if (AssetOwner->PluginOwners.Contains(Plugin))
			{
				AssetOwner->PluginOwners.Remove(Plugin);
				if (AssetOwner->PluginOwners.IsEmpty())
				{
					bRemoveAsset = !AssetOwner->bIsBaseGameAsset;
					if (!bRemoveAsset)
					{
						FAssetRegistryState OldState{}; // Create a temporary AssetRegistryState as this is the only exposed way to update a state without the UObject
						FAssetData* OldGameData = new FAssetData(AssetOwner->BaseGameAssetData);
						OldState.AddAssetData(OldGameData); // needs to be a pointer to a new object! will be destroyed in the FAssetRegistryState destructor 
						AssetRegistryPtr->AppendState(OldState);
					}
					AssetOwners.Remove(AssetPath);
				}
			}
		}
		if (!bRemoveAsset)
		{
			UE_LOG(LogGFPakLoader, Display, TEXT("Not removing asset '%s' from Asset Registry. Flags: [ %s ]"), *AssetPath.ToString(), *PackageFlagsToString(AssetData.PackageFlags))
			return;
		}
		AlreadyRemovedAssets.Add(AssetPath);
		
		TArray<FAssetData> ExistingPackageAssets;
		AssetRegistryPtr->GetAssetsByPackageName(AssetData.PackageName, ExistingPackageAssets, false, false /* See below */);
		// Note: in Cooked Packages, Blueprints have 2 assets within the same package: the Blueprint itself and the BlueprintGeneratedClass '_C'.
		// UE does not support having both in some functions like AssetRegistry.GetAssetsByPackageName, so the default filtering (for cooked packages) will
		// filter out the BP and keep the class as per UE::AssetRegistry::Utils::ShouldSkipAsset
		bool bRemovePackage = (ExistingPackageAssets.Num() <= 1); // If there is only one asset (the current one), we can remove the package
		if (!bRemovePackage)
		{
			ExistingPackageAssets.Remove(AssetData);
			for (auto It = ExistingPackageAssets.CreateIterator(); It; ++It)
			{
				FSoftObjectPath ExistingAssetPath = It->GetSoftObjectPath();
				if (AlreadyRemovedAssets.Contains(ExistingAssetPath))
				{
					It.RemoveCurrent();
				}
			}
			bRemovePackage = ExistingPackageAssets.IsEmpty();
		}
		
		
		UE_CLOG(AssetData.AssetClassPath == UWorld::StaticClass()->GetClassPathName(), LogGFPakLoader, Verbose, TEXT("Removing MAP Asset from Asset Registry: '%s'"), *AssetData.GetObjectPathString())
		UE_CLOG(AssetData.AssetClassPath != UWorld::StaticClass()->GetClassPathName(), LogGFPakLoader, VeryVerbose, TEXT("   Removing Asset from Asset Registry: '%s'"), *AssetData.GetObjectPathString())
		
		UPackage* DeletedObjectPackage = AssetData.PackageName.IsNone() || !bRemovePackage ? nullptr : FindPackage(nullptr, *AssetData.PackageName.ToString()); // do not force load the package
		UObject* AssetToDelete = AssetData.FastGetAsset();

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

			ForEachObjectWithOuter(AssetToDelete, [](UObject* Object)
			{
				Object->RemoveFromRoot();
				Object->ClearFlags(RF_Standalone);
			}, true);
			if (UWorld* World = Cast<UWorld>(AssetToDelete))
			{
				World->CleanupWorld();
			}
		}
		
		if (bRemovePackage)
		{
			OutPackageNamesToRemove.Add(AssetData.PackageName);
			if (DeletedObjectPackage)
			{
				ForEachObjectWithOuter(DeletedObjectPackage, [](UObject* Object)
				{
					Object->RemoveFromRoot();
					Object->ClearFlags(RF_Standalone);
				}, true);
			}
		}
		
		if (AssetToDelete)
		{
			AssetToDelete->MarkPackageDirty();

			// Equivalent of FAssetRegistryModule::AssetDeleted( AssetToDelete ) which is Editor only
			{
				// We might need to find a way to add the package to the EmptyPackageCache as per FAssetRegistryModule::AssetDeleted, but it works without
				FAssetData AssetDataDeleted = FAssetData(AssetToDelete, FAssetData::ECreationFlags::AllowBlueprintClass,
					EAssetRegistryTagsCaller::AssetRegistryQuery);
				// Let subscribers know that the asset was removed from the registry
				AssetRegistryPtr->OnAssetRemoved().Broadcast(AssetDataDeleted);
				AssetRegistryPtr->OnAssetsRemoved().Broadcast({AssetDataDeleted});

				// Notify listeners that an in-memory asset was just deleted
				AssetRegistryPtr->OnInMemoryAssetDeleted().Broadcast(AssetToDelete);
			}
			// Remove standalone flag so garbage collection can delete the object and public flag so that the object is no longer considered to be an asset
			AssetToDelete->ClearFlags(RF_Standalone | RF_Public);
		}
		else
		{
			AssetRegistryPtr->OnAssetRemoved().Broadcast(AssetData);
			AssetRegistryPtr->OnAssetsRemoved().Broadcast({AssetData});
		}

#if WITH_EDITOR
		if (bRemovePackage && DeletedObjectPackage)
		{
			AssetRegistryPtr->PackageDeleted(DeletedObjectPackage);
		}
#endif
	});
	
}

FString UGFPakLoaderSubsystem::PackageFlagsToString(uint32 PackageFlags)
{
	FString FlagsStr;
#define AddToPackageFlagsString(FlagsToCheck) \
	if ( (PackageFlags & FlagsToCheck) != 0 ) \
	{ \
		FlagsStr += (FlagsStr.IsEmpty() ? TEXT("") : TEXT(" | ")) + FString( #FlagsToCheck ); \
	}
	AddToPackageFlagsString(PKG_NewlyCreated);
	AddToPackageFlagsString(PKG_ClientOptional);
	AddToPackageFlagsString(PKG_ServerSideOnly);
	AddToPackageFlagsString(PKG_CompiledIn);
	AddToPackageFlagsString(PKG_ForDiffing);
	AddToPackageFlagsString(PKG_EditorOnly);
	AddToPackageFlagsString(PKG_Developer);
	AddToPackageFlagsString(PKG_Developer);
	AddToPackageFlagsString(PKG_UncookedOnly);
	AddToPackageFlagsString(PKG_Cooked);
	AddToPackageFlagsString(PKG_ContainsNoAsset);
	AddToPackageFlagsString(PKG_NotExternallyReferenceable);
	AddToPackageFlagsString(PKG_UnversionedProperties);
	AddToPackageFlagsString(PKG_ContainsMapData);
	AddToPackageFlagsString(PKG_IsSaving);
	AddToPackageFlagsString(PKG_Compiling);
	AddToPackageFlagsString(PKG_ContainsMap);
	AddToPackageFlagsString(PKG_RequiresLocalizationGather);
	AddToPackageFlagsString(PKG_PlayInEditor);
	AddToPackageFlagsString(PKG_ContainsScript);
	AddToPackageFlagsString(PKG_DisallowExport);
	AddToPackageFlagsString(PKG_CookGenerated);
	AddToPackageFlagsString(PKG_DynamicImports);
	AddToPackageFlagsString(PKG_RuntimeGenerated);
	AddToPackageFlagsString(PKG_ReloadingForCooker);
	AddToPackageFlagsString(PKG_FilterEditorOnly);
#undef AddToPackageFlagsString
	return FlagsStr.IsEmpty() ? TEXT("PKG_None") : FlagsStr;
}
