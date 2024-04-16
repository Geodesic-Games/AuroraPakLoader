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

	UFUNCTION(BlueprintCallable, Category="GameFeatures Pak Loader Settings")
	void SetPakLoadPath(const FString& Path);
	UFUNCTION(BlueprintPure, Category="GameFeatures Pak Loader Settings")
	FString GetAbsolutePakLoadPath() const;

	/**
	 * If true, the GFPakLoaderSubsystem will add all the PakPlugins found in the StartupPakLoadDirectory.
	 */
	UPROPERTY(config, EditAnywhere, Category = "GF Pak Loader")
	bool bAddPakPluginsFromStartupLoadDirectory = true;
	/**
	 * If true, the GFPakLoaderSubsystem will automatically mount all the Pak Plugins added to the subsystem.
	 */
	UPROPERTY(config, EditAnywhere, Category = "GF Pak Loader")
	bool bAutoMountPakPlugins = true;
	/**
	 * If true, the GFPakLoaderSubsystem will automatically activate the GameFeatures of supported PakPlugins, if their settings is to be activated on load.
	 */
	UPROPERTY(config, EditAnywhere, Category = "GF Pak Loader")
	bool bAutoActivateGameFeatures = true;
private:
	/**
	 * The Path to the Pak Plugin Directory to load at startup. Relative to the project directory if inside of it, otherwise this is a relative path.
	 */
	UPROPERTY(config, EditAnywhere, Category = "GF Pak Loader", meta = (AllowPrivateAccess))
	FDirectoryPath StartupPakLoadDirectory;
};
