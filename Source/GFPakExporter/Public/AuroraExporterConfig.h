// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"
#include "AuroraExporterConfig.generated.h"

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Config, meta=(FullyExpand=true))
	TArray<FName> PackagePaths; //todo: allow packages to not export subfolders 
	
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
		return !DLCName.IsEmpty() && !IsEmpty();
	}

	/**
	 * Check if this config is a Plugin DLC config.
	 * A Plugin DLC Config has no Assets and only one PackagePath which is the MountPoint of the Plugin Content.
	 * The config also needs to not point to a DLC Plugin.
	 * @param bEnsureDLCName if true, the DLCName property will also be checked, and it needs to match the MountPoint
	 * @param OutPluginName if it is a PluginDLC, returns the name of the Plugin
	 */
	bool IsPluginDLC(bool bEnsureDLCName = true, FString* OutPluginName = nullptr) const;

	/** Return true if the rules of this Exporter Config should export the given Asset */
	bool ShouldExportAsset(const FAssetData& AssetData) const;
	
	bool LoadJsonConfig(const TSharedPtr<FJsonObject>& JsonObject);
	bool LoadJsonConfig(const FString& InJsonPath);
	bool SaveJsonConfig(const TSharedPtr<FJsonObject>& JsonObject) const;
	bool SaveJsonConfig(const FString& InJsonPath) const;

	static TOptional<FAuroraExporterConfig> FromJsonConfig(const FString& InJsonPath)
	{
		FAuroraExporterConfig Config;
		return Config.LoadJsonConfig(InJsonPath) ? MoveTemp(Config) : TOptional<FAuroraExporterConfig>{};
	}
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

	/** Location where the Config Json will be saved. Leave blank to save in the default location in the Intermediate folder */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category=Settings, meta = (FilePathFilter = "json"))
	FFilePath ConfigFilePath;
};