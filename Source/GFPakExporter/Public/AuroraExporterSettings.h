// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"
#include "AuroraSaveFilePath.h"
#include "AuroraExporterSettings.generated.h"

/**
 * 
 */
USTRUCT(Blueprintable)
struct GFPAKEXPORTER_API FAuroraExporterConfig
{
	GENERATED_BODY()
public:
	/** The name of the exported DLC */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Config)
	FString DLCName;

	/**
	 * List of Folders to include in this export (all their content and subfolders will also be included). They need to start with a MountPoint.
	 * Ex: /Game/Folder/Maps  or /PluginName/Blueprints
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Config, meta=(FullyExpand=true, ContentDir, LongPackageName))
	TArray<FAuroraDirectoryPath> PackagePaths; //todo: allow packages to not export subfolders 
	
	/**
	 * List of Assets to include in this export.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Config, meta=(FullyExpand=true))
	TArray<FSoftObjectPath> Assets;

	/**
	 * If true, all assets that the packaged Assets are dependent on will also be included.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Config, meta=(FullyExpand=true))
	bool bIncludeHardReferences = true; //todo: make the UI display the dependent packages

	bool IsEmpty() const
	{
		return PackagePaths.IsEmpty() && Assets.IsEmpty();
	}

	bool IsValid() const
	{
		return HasValidDLCName() && !IsEmpty();
	}

	bool HasValidDLCName() const;

	/** Return true if the rules of this Exporter Config should export the given Asset */
	bool ShouldExportAsset(const FAssetData& AssetData) const;
	
	FString GetDefaultDLCNameBasedOnContent(const FString& FallbackName) const;
	
	static TOptional<FAuroraExporterConfig> FromPluginName(const FString& InPluginName);
};


/**
 * Available build configurations. Mirorred from EBuildConfiguration.
 */
UENUM(Blueprintable)
enum class EAuroraBuildConfiguration : uint8
{
	/** Unknown build configuration. */
	Unknown,

	/** Debug build. */
	Debug,

	/** DebugGame build. */
	DebugGame,

	/** Development build. */
	Development,

	/** Shipping build. */
	Shipping,

	/** Test build. */
	Test
};

/**
 * 
 */
USTRUCT(Blueprintable)
struct GFPAKEXPORTER_API FAuroraExporterSettings
{
	GENERATED_BODY()
public:
	FAuroraExporterSettings() = default;
	FAuroraExporterSettings(const FAuroraExporterSettings&) = default;
	
	explicit FAuroraExporterSettings(const FAuroraExporterConfig& InConfig) : Config(InConfig) {}
	
	/** The Configuration for the Export */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Config, meta=(ShowOnlyInnerProperties, FullyExpand=true, NoResetToDefault))
	FAuroraExporterConfig Config;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	EAuroraBuildConfiguration BuildConfiguration = EAuroraBuildConfiguration::Development;
	EBuildConfiguration GetBuildConfiguration() const { return static_cast<EBuildConfiguration>(BuildConfiguration); }
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	EAuroraBuildConfiguration CookConfiguration = EAuroraBuildConfiguration::Development;
	EBuildConfiguration GetCookConfiguration() const { return static_cast<EBuildConfiguration>(CookConfiguration); }
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	bool bCookUnversioned = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	bool bBuildUAT = false;

	/** Location where the Config Json will be saved. Leave blank to save in the default location in the Intermediate folder */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Transient, AdvancedDisplay, Category=Settings, meta = (FilePathFilter = "json"))
	FAuroraSaveFilePath SettingsFilePath;

	bool LoadJsonSettings(const TSharedPtr<FJsonObject>& JsonObject);
	bool LoadJsonSettings(const FString& InJsonPath);
	bool SaveJsonSettings(const TSharedPtr<FJsonObject>& JsonObject) const;
	bool SaveJsonSettings(const FString& InJsonPath) const;

	static TOptional<FAuroraExporterSettings> FromJsonSettings(const FString& InJsonPath)
	{
		FAuroraExporterSettings Settings;
		return Settings.LoadJsonSettings(InJsonPath) ? MoveTemp(Settings) : TOptional<FAuroraExporterSettings>{};
	}
};