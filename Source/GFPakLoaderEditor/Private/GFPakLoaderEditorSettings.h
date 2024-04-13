// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "GFPakLoaderEditorSettings.generated.h"

UCLASS(config = EditorPerProjectUserSettings, defaultconfig, meta = (DisplayName = "GF Pak Loader"))
class GFPAKLOADEREDITOR_API UGFPakLoaderEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGFPakLoaderEditorSettings();

	//~Begin UDeveloperSettings
	virtual FName GetCategoryName() const override { return FName("Plugins"); }
	virtual FName GetSectionName() const override { return FName("GF Pak Loader"); }
	virtual FText GetSectionText() const override { return NSLOCTEXT("GFPakLoaderEditor", "GFPakLoaderName", "GF Pak Loader"); }
	virtual FText GetSectionDescription() const override { return NSLOCTEXT("GFPakLoaderEditor", "GFPakLoaderDescription", "Editor configuration settings for the GF Pak Loader plugin."); }
	//~End UDeveloperSettings

	UPROPERTY(config, EditAnywhere, Category = "GF Pak Loader")
	FDirectoryPath BuildOutputDirectory;

	UPROPERTY(config, EditAnywhere, Category = "GF Pak Loader")
	FString StagingDirectoryName;

	UPROPERTY(config, EditAnywhere, Category = "GF Pak Loader")
	bool bCompressPaks;
};
