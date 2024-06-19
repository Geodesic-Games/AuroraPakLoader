// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"

#include "Engine/DeveloperSettings.h"
#include "GFPakLoaderSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "GF Pak Loader"))
class GFPAKLOADER_API UGFPakLoaderSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGFPakLoaderSettings();

	//~Begin UDeveloperSettings
	virtual FName GetCategoryName() const override { return FName("Plugins"); }
	virtual FName GetSectionName() const override { return FName("GF Pak Loader"); }
#if WITH_EDITOR
	virtual FText GetSectionText() const override { return NSLOCTEXT("GFPakLoader", "GFPakLoaderName", "GF Pak Loader"); }
	virtual FText GetSectionDescription() const override { return NSLOCTEXT("GFPakLoader", "GFPakLoaderDescription", "Runtime configuration settings for the GF Pak Loader plugin."); }
	
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UDeveloperSettings

	//todo: add a directory watcher and an option to auto watch the PakLoadPath
	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader Settings")
	void SetPakLoadPath(const FString& Path);
	UFUNCTION(BlueprintPure, Category="GameFeatures Pak Loader Settings")
	FString GetAbsolutePakLoadPath() const;

	/**
	 * If true, the GFPakLoaderSubsystem will add all the PakPlugins found in the StartupPakLoadDirectory.
	 */
	UPROPERTY(config, EditAnywhere, Category = "GF Pak Loader", meta = (DisplayPriority=2))
	bool bAddPakPluginsFromStartupLoadDirectory = true; //todo: now that this is an engine subsystem, we should assess when this should run (editor start or PIE start) and if we need a different variable for each
	/**
	 * If true, the GFPakLoaderSubsystem will automatically mount all the Pak Plugins added to the subsystem.
	 */
	UPROPERTY(config, EditAnywhere, Category = "GF Pak Loader", meta = (DisplayPriority=3))
	bool bAutoMountPakPlugins = true;
	/**
	 * Any potential Pak folder found that starts with this prefix will be ignored and not registered with the subsystem,
	 * unless calling UGFPakLoaderSubsystem::GetOrAddPakPlugin directly
	 */
	UPROPERTY(config, EditAnywhere, Category = "GF Pak Loader", meta = (DisplayPriority=1))
	FString IgnorePakWithPrefix = TEXT("_");
	/**
	 * If true, the GFPakLoaderSubsystem will automatically activate the GameFeatures of supported PakPlugins, if their settings is to be activated on load.
	 */
	UPROPERTY(config, EditAnywhere, Category = "GF Pak Loader", meta = (DisplayPriority=4))
	bool bAutoActivateGameFeatures = true;
	/**
	 * If true, Paks without .uplugin will not ne loaded, avoiding potential clashes with project content
	 */
	UPROPERTY(config, EditAnywhere, Category = "GF Pak Loader", meta = (DisplayPriority=5), AdvancedDisplay)
	bool bRequireUPluginPaks = true;
private:
	/**
	 * The Path to the Pak Plugin Directory to load at startup. Relative to the project directory if inside of it, otherwise this is a relative path.
	 */
	UPROPERTY(config, EditAnywhere, Category = "GF Pak Loader", meta = (AllowPrivateAccess, DisplayPriority=0))
	FDirectoryPath StartupPakLoadDirectory;
};
