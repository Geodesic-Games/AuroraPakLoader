// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ILauncher.h"
#include "Templates/SharedPointer.h"

/**
 * 
 */
class GFPAKEXPORTER_API FGFPakExporterAssetFolderContextMenu : public TSharedFromThis<FGFPakExporterAssetFolderContextMenu>
{
public:
	void Initialize();
	void Shutdown();
	
private:
	/** Delegate handler to delay execution of menu extension until slate is ready */
	void RegisterMenus();

	/** Delegate to extend the content browser asset context menu */
	void PopulateAssetFolderContextMenu(UToolMenu* InMenu) const;
	
	/** Delegate to extend the content browser asset context menu */
	void PopulateAssetFileContextMenu(UToolMenu* InMenu);

	/** Extends menu with storm sync actions (when only files are selected) */
	void AddFileMenuOptions(UToolMenu* InMenu, const TArray<FName>& InSelectedPackageNames);
	
	/** Extends menu with storm sync actions (when folders are selected) */
	static void AddFolderMenuOptions(UToolMenu* InMenu, const TArray<FString>& InSelectedPackagePaths, const TArray<FString>& InSelectedAssetPackages);

	/** Asks asset registry for all the discovered files in the passed in paths (eg. folders) */
	static void GetSelectedAssetsInPaths(const TArray<FString>& InPaths, TArray<FName>& OutSelectedPackageNames);

	/** Helper to return the list of selected assets and folders */
	void GetSelectedFilesAndFolders(const UToolMenu* InMenu, TArray<FString>& OutSelectedPackagePaths, TArray<FString>& OutSelectedAssetPackages) const;

	/** Export a list of assets locally as a storm sync archive file */
	static void ExecuteCreateDLCAction(TArray<TSharedRef<IPlugin>> InPlugins);

	static TArray<TSharedRef<IPlugin>> GetPluginsFromPackagePaths(TArray<FString> InPackagePaths);
	
	static void MessageReceived(const FString& InMessage);
	static void HandleStageStarted(const FString& InStage);
	static void HandleStageCompleted(const FString& InStage, double StageTime);
	static void LaunchCompleted(bool Outcome, double ExecutionTime, int32 ReturnCode);
	static void LaunchCanceled(double ExecutionTime);

	static ILauncherPtr Launcher;
	static ILauncherWorkerPtr LauncherWorker;
};
