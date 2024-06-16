// Copyright GeoTech BV


#include "GFPakLoaderSubsystem.h"

#include "GFPakLoaderPlatformFile.h"
#include "GFPakLoaderLog.h"
#include "GFPakLoaderSettings.h"
#include "Algo/Find.h"
#include "Engine/AssetManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"


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
	for (UGFPakPlugin* PakPlugin : GameFeaturesPakPlugins)
	{
		PakPlugin->Deinitialize();
		OnPakPluginRemovedDelegate.Broadcast(PakPlugin);
		PakPlugin->OnStatusChanged().RemoveAll(this);
	}
	GameFeaturesPakPlugins.Empty();
	
	// We restore the original delegate. See UGFPakLoaderSubsystem::Initialize for more explanations
	IPluginManager::Get().SetRegisterMountPointDelegate(IPluginManager::FRegisterMountPointDelegate::CreateStatic(&FPackageName::RegisterMountPoint));

	FWorldDelegates::OnStartGameInstance.RemoveAll(this);
	
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
		UE_LOG(LogGFPakLoader, Error, TEXT("Unable to load the Pak Plugin folder because the directory does not exist: '%s' (Folder requested: '%s')"), *PakPluginFolder, *PakPluginFolderOriginal)
		return;
	}
	
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
	
	UGFPakPlugin** ExistingPakPlugin = Algo::FindByPredicate(GameFeaturesPakPlugins,
		[&PakPluginPath](const UGFPakPlugin* PakPlugin)
		{
			return FPaths::IsSamePath(PakPlugin->GetPakPluginDirectory(), PakPluginPath);
		});

	bIsNewlyAdded = !ExistingPakPlugin;
	if (bIsNewlyAdded)
	{
		UGFPakPlugin* PakPlugin = NewObject<UGFPakPlugin>(this);
		PakPlugin->SetPakPluginDirectory(PakPluginPath);
		if (PakPlugin->LoadPluginData())
		{
			GameFeaturesPakPlugins.Emplace(PakPlugin);

			PakPlugin->OnStatusChanged().AddDynamic(this, &ThisClass::PakPluginStatusChanged);
			OnPakPluginAddedDelegate.Broadcast(PakPlugin);
			OnPakPluginStatusChangedDelegate.Broadcast(PakPlugin, EGFPakLoaderStatus::NotInitialized, PakPlugin->GetStatus());
			
			return PakPlugin;
		}
		PakPlugin->ConditionalBeginDestroy();
		return nullptr;
	}

	return *ExistingPakPlugin;
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

FString UGFPakLoaderSubsystem::CollapseRelativeDirectories(FString Filename)
{
	const static FString ParentFolder{TEXT("../")};
	
	int NbParents = 0;
	while (Filename.RemoveFromStart(ParentFolder))
	{
		++NbParents;
	}
	FPaths::CollapseRelativeDirectories(Filename);
	while (NbParents > 0)
	{
		Filename = ParentFolder + Filename;
		--NbParents;
	}
	FPaths::RemoveDuplicateSlashes(Filename);
	return Filename;
}

UGFPakPlugin* UGFPakLoaderSubsystem::FindMountedPakContainingFile(const TCHAR* OriginalFilename, FString* PakAdjustedFilename)
{
	const FName FilenameF {OriginalFilename};
	const FString Extension {FPaths::GetExtension(OriginalFilename, true)};
	TArray<UGFPakPlugin*> Plugins = GetPakPluginsWithStatusAtLeast(EGFPakLoaderStatus::Mounted); //todo: replace with some sort of enumerator
	for (UGFPakPlugin* Plugin : Plugins)
	{
		if (const TSharedPtr<const FGFPakFilenameMap>* MountedFilenameFound = Plugin->GetPakFilenamesMap().Find(FilenameF))
		{
			if (const TSharedPtr<const FGFPakFilenameMap>& MountedFilename = *MountedFilenameFound)
			{
				if (PakAdjustedFilename)
				{
					*PakAdjustedFilename = MountedFilename->AdjustedFullFilename;
				}
				return Plugin;
			}
		}
	}
	
	if (PakAdjustedFilename)
	{
		PakAdjustedFilename->Empty();
	}
	return nullptr;
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

void UGFPakLoaderSubsystem::Start()
{
	if (!IsReady() || bStarted)
	{
		return;
	}
	bStarted = true;
	UE_LOG(LogGFPakLoader, Verbose, TEXT(" UGFPakLoaderSubsystem Starting..."))
	
	// The GameFeatureSubsystem technically handles the registering and the mounting of the Plugin with the IPluginManager, but by doing so, it ends up
	// creating a new Mount point for our assets to be mounted.
	// Our packaged assets do no exist at a specific location on disk, so we need the Mount Point to match their path in the Pak file which is relative:
	//   => '../../../StatcastUnreal/Plugins/GameFeatures/GFPlugin/Content/' mapped as  '/GFPlugin/'
	// The problem is that if we let the PluginManager create the MountPoint, it will end up being an absolute path and the assets will not resolve:
	//   => 'D:/path/StatcastUnreal/Stadiums/GFPlugin/Content/' mounted to '/GFPlugin/'
	// Because the Mount Point would be created everytime the GameFeatures is activated, we need to ensure it is not created in the first place.
	// As per the IPluginManager and GameFeatureSubsystem code, setting the plugin to not be ExplicitlyLoaded will stop it from automatically creating a Mount Point, but will make other parts not work properly.
	// Instead, we override the IPluginManager::RegisterMountPointDelegate with our own where we stop the creation of a mount point for one of our Pak Plugin
	// and we restore the regular delegate when the subsystem is destroyed.
	IPluginManager::Get().SetRegisterMountPointDelegate(IPluginManager::FRegisterMountPointDelegate::CreateUObject(this, &UGFPakLoaderSubsystem::RegisterMountPoint));
			
	OnSubsystemReadyDelegate.Broadcast();
	UE_LOG(LogGFPakLoader, Verbose, TEXT("...Initialized the UGFPakLoaderSubsystem"))
	
	if (GetPakLoaderSettings()->bAddPakPluginsFromStartupLoadDirectory)
	{
		const FString Path = GetDefaultPakPluginFolder();
		if (FPaths::DirectoryExists(Path))
		{
			TArray<UGFPakPlugin*> NewPlugins;
			TArray<UGFPakPlugin*> AllPlugins;
			AddPakPluginFolder(Path, NewPlugins, AllPlugins);
		}
		else
		{
			UE_LOG(LogGFPakLoader, Warning, TEXT("The default Pak folder location from the GF Pak Loader Settings does not exist: '%s'"), *Path)
		}
	}
}

void UGFPakLoaderSubsystem::PakPluginStatusChanged(UGFPakPlugin* PakPlugin, EGFPakLoaderPreviousStatus OldValue, EGFPakLoaderStatus NewValue)
{
	OnPakPluginStatusChangedDelegate.Broadcast(PakPlugin, OldValue, NewValue);
}

void UGFPakLoaderSubsystem::RegisterMountPoint(const FString& RootPath, const FString& ContentPath)
{
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
	UE_LOG(LogGFPakLoader, Log, TEXT("UGFPakLoaderSubsystem::RegisterMountPoint: About to register MountPoint  '%s' => '%s'"), *RootPath, *ContentPath)
	FPackageName::RegisterMountPoint(RootPath, ContentPath);
}

void UGFPakLoaderSubsystem::Debug_LogPaths()
{
	UE_LOG(LogGFPakLoader, Warning, TEXT(" === TEST OF FPaths Functions IN '%s' ==="), GIsEditor ? TEXT("EDITOR") : TEXT("GAME") );
	FString BaseDir = FPlatformProcess::BaseDir();
	FString RootDir = FPlatformMisc::RootDir();
	FString ProjectDir = FPlatformMisc::ProjectDir();
	UE_LOG(LogGFPakLoader, Warning, TEXT("  BaseDir         '%s' "), *BaseDir);
	UE_LOG(LogGFPakLoader, Warning, TEXT("  RootDir         '%s' "), *RootDir);
	UE_LOG(LogGFPakLoader, Warning, TEXT("  ProjectDir      '%s' "), *ProjectDir);
	UE_LOG(LogGFPakLoader, Warning, TEXT("  Full ProjectDir '%s' "), *FPaths::ConvertRelativePathToFull(ProjectDir));
		
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

		FString Collapsed = UGFPakLoaderSubsystem::CollapseRelativeDirectories(FilenameTest);
			
		UE_LOG(LogGFPakLoader, Warning, TEXT("  - Test for Path ----------------    '%s'"), *FilenameTest);
		UE_LOG(LogGFPakLoader, Warning, TEXT("      =>  MakeStandardFilename        '%s'"), *StandardFilenameTest);
		UE_LOG(LogGFPakLoader, Warning, TEXT("      => ConvertRelativePathToFull    '%s'"), *Standardized);
		UE_LOG(LogGFPakLoader, Warning, TEXT("      => CollapseRelativeDirectories  '%s'"), *Collapsed);
	}
}
