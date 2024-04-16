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

	//todo: how to check if it was already started?
	FWorldDelegates::OnStartGameInstance.AddUObject(this, &UGFPakLoaderSubsystem::OnGameInstanceStarted);

	if (GIsRunning)
	{
		UE_LOG(LogGFPakLoader, Verbose, TEXT(" Not waiting for FCoreDelegates::OnFEngineLoopInitComplete because GIsRunning returned true"))
		bEngineLoopInitCompleted = true;
	}
	else
	{
		FCoreDelegates::OnFEngineLoopInitComplete.AddUObject(this, &UGFPakLoaderSubsystem::OnEngineLoopInitCompleted);
	}

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
	FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);
	
	bGameInstanceStarted = false;
	bAssetManagerCreated = false;
	bStarted = false;
	UE_LOG(LogGFPakLoader, Verbose, TEXT("...Deinitialized the UGFPakLoaderSubsystem"))
	OnSubsystemShutdownDelegate.Broadcast();
}

FString UGFPakLoaderSubsystem::GetDefaultPakPluginFolder() const
{
	const UGFPakLoaderSettings* PakLoaderSettings = GetDefault<UGFPakLoaderSettings>();
	return PakLoaderSettings->GetAbsolutePakLoadPath();
}

TArray<UGFPakPlugin*> UGFPakLoaderSubsystem::AddPakPluginFolder(const FString& InPakPluginFolder)
{
	FString PakPluginFolder = InPakPluginFolder.IsEmpty() ? GetDefaultPakPluginFolder() : InPakPluginFolder;
	if (!IsReady())
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("UGFPakLoaderSubsystem is not ready. Unable to AddPakPluginFolder '%s'"), *PakPluginFolder)
		return {};
	}
	if (!FPaths::DirectoryExists(PakPluginFolder))
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("Unable to load the Pak Plugin folder because the directory does not exist: '%s'"), *PakPluginFolder)
		return {};
	}
	
	UE_LOG(LogGFPakLoader, Log, TEXT("Adding Pak Plugins located in the folder '%s'..."), *PakPluginFolder)
	const int NbPrePlugins = GameFeaturesPakPlugins.Num();
	
	IFileManager& FileManager = IFileManager::Get();
	FDirectoryLister DirectoryLister;
	FileManager.IterateDirectory(*PakPluginFolder, DirectoryLister);

	int NbFailed = 0;
	TArray<UGFPakPlugin*> Plugins;
	for (const FString& PluginPath : DirectoryLister.Directories)
	{
		if (UGFPakPlugin* Plugin = AddPakPlugin(PluginPath))
		{
			Plugins.Add(Plugin);
		}
		else
		{
			++NbFailed;
		}
	}

	const int NbAddedPlugins = GameFeaturesPakPlugins.Num() - NbPrePlugins;
	const int NbReferencedPlugins = Plugins.Num() - NbAddedPlugins;
	UE_CLOG(NbAddedPlugins > 0, LogGFPakLoader, Log, TEXT("... %d Pak Plugins were added, %d Plugins were already referenced by the subsystem, and %d folders were not valid Pak Plugin folders"),
		NbAddedPlugins, NbReferencedPlugins, NbFailed)
	UE_CLOG(NbAddedPlugins == 0, LogGFPakLoader, Verbose, TEXT("... %d Pak Plugins were added, %d Plugins were already referenced by the subsystem, and %d folders were not valid Pak Plugin folders"),
		NbAddedPlugins, NbReferencedPlugins, NbFailed)
	return Plugins;
}

UGFPakPlugin* UGFPakLoaderSubsystem::AddPakPlugin(const FString& InPakPluginPath)
{
	if (!IsReady())
	{
		UE_LOG(LogGFPakLoader, Error, TEXT("UGFPakLoaderSubsystem is not ready. Unable to AddPakPlugin '%s'"), *InPakPluginPath)
		return {};
	}
	
	UGFPakPlugin** ExistingPakPlugin = Algo::FindByPredicate(GameFeaturesPakPlugins,
		[&InPakPluginPath](const UGFPakPlugin* PakPlugin)
		{
			return FPaths::IsSamePath(PakPlugin->GetPakPluginDirectory(), InPakPluginPath);
		});
	
	if (!ExistingPakPlugin)
	{
		UGFPakPlugin* PakPlugin = NewObject<UGFPakPlugin>(this);
		PakPlugin->SetPakPluginDirectory(InPakPluginPath);
		if (PakPlugin->LoadPluginData())
		{
			GameFeaturesPakPlugins.Emplace(PakPlugin);

			PakPlugin->OnStatusChanged().AddDynamic(this, &ThisClass::PakPluginStatusChanged);
			OnPakPluginAddedDelegate.Broadcast(PakPlugin);
			OnPakPluginStatusChangedDelegate.Broadcast(PakPlugin, EGFPakLoaderStatus::NotInitialized, PakPlugin->GetStatus());

			if (GetDefault<UGFPakLoaderSettings>()->bAutoMountPakPlugins && PakPlugin->GetStatus() == EGFPakLoaderStatus::Unmounted)
			{
				PakPlugin->Mount();
			}
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
		UE_LOG(LogGFPakLoader, Log, TEXT("     => Added %sMountPoint:  '%s' => '%s'"),
				ContentMountPoint->NeedsUnregistering() ? TEXT("") : TEXT("Existing "), *ContentMountPoint->GetRootPath(), *ContentMountPoint->GetContentPath())
		
		// Here we create a shared Pointer with a custom Deleter as we statically keep track of the MountPoints.
		TSharedPtr<FPluginMountPoint> MountPoint(new FPluginMountPoint(MoveTemp(ContentMountPoint.GetValue())),
			[InContentPath, WeakSubsystem = TWeakObjectPtr<UGFPakLoaderSubsystem>(this)](FPluginMountPoint* DeletedMountPoint)
		{
			if (ensure(DeletedMountPoint))
			{
				MountPoints.Remove(DeletedMountPoint->GetContentPath());
				UE_LOG(LogGFPakLoader, Log, TEXT("Deleted FPluginMountPoint from Subsystem:  '%s' => '%s'"), *DeletedMountPoint->GetRootPath(), *DeletedMountPoint->GetContentPath())
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

void UGFPakLoaderSubsystem::OnGameInstanceStarted(UGameInstance* GameInstance)
{
	if (GameInstance == GetGameInstance())
	{
		bGameInstanceStarted = true;
		FWorldDelegates::OnStartGameInstance.RemoveAll(this);
		UE_LOG(LogGFPakLoader, Verbose, TEXT(" FWorldDelegates::OnStartGameInstance called"))
		Start();
	}
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
	FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);
	UE_LOG(LogGFPakLoader, Verbose, TEXT(" FCoreDelegates::OnFEngineLoopInitComplete called"))
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

	const FString Path = GetDefaultPakPluginFolder();
	if (FPaths::DirectoryExists(Path))
	{
		AddPakPluginFolder(Path);
	}
	else
	{
		UE_LOG(LogGFPakLoader, Warning, TEXT("The default Pak folder location from the GF Pak Loader Settings does not exist: '%s'"), *Path)
	}
	UE_LOG(LogGFPakLoader, Verbose, TEXT("...Initialized the UGFPakLoaderSubsystem"))
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
