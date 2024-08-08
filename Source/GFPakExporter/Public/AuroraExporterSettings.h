// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"
#include "AuroraSaveFilePath.h"
#include "AuroraExporterSettings.generated.h"

/**
 * 
 */
USTRUCT(Blueprintable)
struct GFPAKEXPORTER_API FAuroraContentDLCExporterConfig
{
	GENERATED_BODY()
public:
	/**
	 * The name of the exported DLC.
	 * Note that if a DLC has the same Name as another Plugin or another DLC, it will fail to load as a DLC is registered with the Plugin Manager which requires unique names
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Config)
	FString DLCName;

	/**
	 * List of Folders to include in this export (all their content and subfolders will also be included). They need to start with a MountPoint.
	 * Ex: /Game/Folder/Maps  or /PluginName/Blueprints
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Included Assets", meta=(FullyExpand=true, ContentDir, LongPackageName))
	TArray<FAuroraDirectoryPath> PackagePaths; //todo: allow packages to not export subfolders 
	
	/**
	 * List of Assets to include in this export.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Included Assets", meta=(FullyExpand=true))
	TArray<FSoftObjectPath> Assets;

	/**
	 * If true, all assets that the packaged Assets are dependent on will also be included.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Included Assets", meta=(FullyExpand=true))
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
	
	static TOptional<FAuroraContentDLCExporterConfig> FromPluginName(const FString& InPluginName);
};


/**
 * 
 */
USTRUCT(Blueprintable)
struct GFPAKEXPORTER_API FAuroraBaseGameExporterConfig
{
	GENERATED_BODY()
public:
	/**
	 * List of Folders to include in this export (all their content and subfolders will also be included). They need to start with a MountPoint.
	 * Ex: /Game/Folder/Maps  or /PluginName/Blueprints
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Additional Included Assets", meta=(FullyExpand=true, ContentDir, LongPackageName))
	TArray<FAuroraDirectoryPath> PackagePathsToInclude; //todo: allow packages to not export subfolders 
	
	/**
	 * List of Assets to include in this export.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Additional Included Assets", meta=(FullyExpand=true))
	TArray<FSoftObjectPath> AssetsToInclude;

	/**
	 * List of Folders to exclude in this export (all their content and subfolders will also be excluded). They need to start with a MountPoint.
	 * Ex: /Game/Folder/Maps  or /PluginName/Blueprints
	 * Important: if the assets inside the folders are referenced by other assets to be cooked, they will still be included in the final cook
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Additional Excluded Assets", meta=(FullyExpand=true, ContentDir, LongPackageName))
	TArray<FAuroraDirectoryPath> PackagePathsToExclude; //todo: allow packages to not export subfolders 
	
	/**
	 * List of Assets to exclude in this export.
	 * Important: if the assets inside the folders are referenced by other assets to be cooked, they will still be included in the final cook
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Additional Excluded Assets", meta=(FullyExpand=true))
	TArray<FSoftObjectPath> AssetsToExclude;

	bool IsEmpty() const
	{
		return PackagePathsToInclude.IsEmpty() && AssetsToInclude.IsEmpty() &&
			PackagePathsToExclude.IsEmpty() && AssetsToExclude.IsEmpty();
	}

	enum class EAssetExportRule : uint8
	{
		Unknown,
		Include,
		Exclude,
	};
	/**
	 * Return the Export Rule for this asset.
	 * An Asset directly listed in `AdditionalAssetsTo *Include*` will have priority over `AdditionalAssetsTo *Exclude*`.
	 * If it is not listed there, `AdditionalPackagePathsTo *Include*` will have priority over `AdditionalPackagePathsTo *Exclude*`
	 */
	EAssetExportRule GetAssetExportRule(FName PackageName) const;
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
struct GFPAKEXPORTER_API FAuroraBuildSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Output)
	FDirectoryPath PackageDirectory;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	EAuroraBuildConfiguration BuildConfiguration = EAuroraBuildConfiguration::Development;
	EBuildConfiguration GetBuildConfiguration() const { return static_cast<EBuildConfiguration>(BuildConfiguration); }
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	EAuroraBuildConfiguration CookConfiguration = EAuroraBuildConfiguration::Development;
	EBuildConfiguration GetCookConfiguration() const { return static_cast<EBuildConfiguration>(CookConfiguration); }

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	FString CookingPlatform = TEXT("Windows"); //todo: find a way to give a dropdown or something nicer. Check FLauncherProfile::AddCookedPlatform
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	bool bCookUnversioned = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	bool bBuildUAT = false;
};


/**
 * 
 */
USTRUCT(Blueprintable)
struct GFPAKEXPORTER_API FAuroraContentDLCExporterSettings
{
	GENERATED_BODY()
public:
	FAuroraContentDLCExporterSettings() = default;
	FAuroraContentDLCExporterSettings(const FAuroraContentDLCExporterSettings&) = default;
	
	explicit FAuroraContentDLCExporterSettings(const FAuroraContentDLCExporterConfig& InConfig) : Config(InConfig) {}
	explicit FAuroraContentDLCExporterSettings(const FAuroraContentDLCExporterConfig& InConfig, const FAuroraBuildSettings& InBuildSettings)
		: Config(InConfig), BuildSettings(InBuildSettings) {}
	
	/** The Configuration for the Export */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Config, meta=(ShowOnlyInnerProperties, FullyExpand=true, NoResetToDefault))
	FAuroraContentDLCExporterConfig Config;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(ShowOnlyInnerProperties, FullyExpand=true, NoResetToDefault))
	FAuroraBuildSettings BuildSettings;
	
	/** Location where the Config Json will be saved. Leave blank to save in the default location in the Intermediate folder */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Transient, AdvancedDisplay, Category=Settings, meta = (FilePathFilter = "json"))
	FAuroraSaveFilePath SettingsFilePath;
	
	bool LoadJsonSettings(const TSharedPtr<FJsonObject>& JsonObject);
	bool LoadJsonSettings(const FString& InJsonPath);
	bool SaveJsonSettings(const TSharedPtr<FJsonObject>& JsonObject) const;
	bool SaveJsonSettings(const FString& InJsonPath) const;
	
	static TOptional<FAuroraContentDLCExporterSettings> FromJsonSettings(const FString& InJsonPath)
	{
		FAuroraContentDLCExporterSettings Settings;
		return Settings.LoadJsonSettings(InJsonPath) ? MoveTemp(Settings) : TOptional<FAuroraContentDLCExporterSettings>{};
	}
};


/**
 * 
 */
USTRUCT(Blueprintable)
struct GFPAKEXPORTER_API FAuroraBaseGameExporterSettings
{
	GENERATED_BODY()
public:
	FAuroraBaseGameExporterSettings() = default;
	FAuroraBaseGameExporterSettings(const FAuroraBaseGameExporterSettings&) = default;
	
	explicit FAuroraBaseGameExporterSettings(const FAuroraBaseGameExporterConfig& InConfig) : Config(InConfig) {}
	
	/** The Configuration for the Export */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Config, meta=(ShowOnlyInnerProperties, FullyExpand=true, NoResetToDefault))
	FAuroraBaseGameExporterConfig Config;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(ShowOnlyInnerProperties, FullyExpand=true, NoResetToDefault))
	FAuroraBuildSettings BuildSettings;
	
	/** Location where the Config Json will be saved. Leave blank to save in the default location in the Intermediate folder */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Transient, AdvancedDisplay, Category=Settings, meta = (FilePathFilter = "json"))
	FAuroraSaveFilePath SettingsFilePath;
	
	bool LoadJsonSettings(const TSharedPtr<FJsonObject>& JsonObject);
	bool LoadJsonSettings(const FString& InJsonPath);
	bool SaveJsonSettings(const TSharedPtr<FJsonObject>& JsonObject) const;
	bool SaveJsonSettings(const FString& InJsonPath) const;
	
	static TOptional<FAuroraBaseGameExporterSettings> FromJsonSettings(const FString& InJsonPath)
	{
		FAuroraBaseGameExporterSettings Settings;
		return Settings.LoadJsonSettings(InJsonPath) ? MoveTemp(Settings) : TOptional<FAuroraBaseGameExporterSettings>{};
	}
};