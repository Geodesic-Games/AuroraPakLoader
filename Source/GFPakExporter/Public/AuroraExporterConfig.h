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
	UPROPERTY(BlueprintReadWrite)
	FString DLCName;
	
	/** List of Plugin Names to include in this export */
	UPROPERTY(BlueprintReadWrite)
	TArray<FName> Plugins;

	/**
	 * List of Folders to include in this expor (all their content and subfolders will also be included). They need to start with a MountPoint.
	 * Ex: /Game/Folder/Maps  or /PluginName/Blueprints
	 */
	UPROPERTY(BlueprintReadWrite)
	TArray<FName> PackagePaths;
	
	/**
	 * List of Assets to include in this export.
	 */
	UPROPERTY(BlueprintReadWrite)
	TArray<FSoftObjectPath> Assets;

	bool IsEmpty() const
	{
		return Plugins.IsEmpty() && PackagePaths.IsEmpty() && Assets.IsEmpty();
	}

	bool IsValid() const
	{
		return !DLCName.IsEmpty() && !IsEmpty();
	}

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
