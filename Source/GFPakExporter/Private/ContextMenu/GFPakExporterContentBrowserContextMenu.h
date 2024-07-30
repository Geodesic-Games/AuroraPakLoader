// Copyright GeoTech BV

#pragma once

#include "ILauncher.h"
#include "Templates/SharedPointer.h"

struct FAuroraExporterConfig;
/**
 * 
 */
class GFPAKEXPORTER_API FGFPakExporterContentBrowserContextMenu : public TSharedFromThis<FGFPakExporterContentBrowserContextMenu>
{
public:
	void Initialize();
	void Shutdown();
	
private:
	/** Delegate handler to delay execution of menu extension until slate is ready */
	void RegisterMenus();

	/** Delegate to extend the content browser asset context menu */
	void PopulateContextMenu(UToolMenu* InMenu, bool bIsAssetMenu) const;

	/** Helper to return the list of selected assets and folders */
	static void GetSelectedFilesAndFolders(const UToolMenu* InMenu, TArray<FString>& OutSelectedPackagePaths, TArray<FSoftObjectPath>& OutSelectedAssets);

	static void ExecuteCreateAuroraContentDLCAction(FAuroraExporterConfig InConfig);

	/**
	 * Return a the list of selected plugins out of the given Package and Asset Paths
	 * @param InSelectedPackagePaths List of PackagePaths
	 * @param InSelectedAssets List of Assets
	 * @param OutSelectedPlugins Return only the Plugins that are directly selected (meaning one of the InSelectedPackagePaths is the MountPoint of this plugin)
	 * @param OutAllPlugins Return all the Plugins that have Package or Assets Paths listed in InSelectedPackagePaths or in InSelectedAssets
	 */
	static void GetPluginsFromSelectedFilesAndFolders(const TArray<FString>& InSelectedPackagePaths, const TArray<FSoftObjectPath>& InSelectedAssets,
		TArray<TSharedRef<IPlugin>>& OutSelectedPlugins, TArray<TSharedRef<IPlugin>>& OutAllPlugins);
};
