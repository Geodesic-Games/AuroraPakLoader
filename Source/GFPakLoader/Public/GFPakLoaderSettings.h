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

	void SetPakLoadPath(const FString& Path);
	FString GetAbsolutePakLoadPath() const;
private:
	/**
	 * The Path to the Pak Plugin Directory to load at startup. Relative to the project directory if inside of it, otherwise this is a relative path.
	 */
	UPROPERTY(config, EditAnywhere, Category = "GF Pak Loader", meta = (AllowPrivateAccess))
	FDirectoryPath StartupPakLoadDirectory;
};
