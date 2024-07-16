// Copyright GeoTech BV


#include "AuroraExporterConfig.h"

#include "GFPakExporterLog.h"
#include "JsonObjectConverter.h"
#include "PluginUtils.h"


bool FAuroraExporterConfig::ShouldExportAsset(const FAssetData& AssetData) const
{
	if (Assets.Contains(AssetData.GetSoftObjectPath()))
	{
		return true;
	}
	
	const FName MountPoint = FPackageName::GetPackageMountPoint(AssetData.PackagePath.ToString());
	if (Plugins.Contains(MountPoint))
	{
		return true;
	}
	
	return PackagePaths.ContainsByPredicate([&AssetData](const FName& ContentFolder)
	{
		return FPaths::IsUnderDirectory(AssetData.PackagePath.ToString(), ContentFolder.ToString());
	});
}

bool FAuroraExporterConfig::LoadJsonConfig(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject)
	{
		return false;
	}
	
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), StaticStruct(), this);
}

bool FAuroraExporterConfig::LoadJsonConfig(const FString& InJsonPath)
{
	if (InJsonPath.IsEmpty())
	{
		return false;
	}
	FString Filename = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*InJsonPath); //For Game builds to debug Sandbox File Paths
	FString DebugAdjusted = InJsonPath == Filename ? FString{} : FString::Printf(TEXT(" (adjusted to '%s')"), *Filename);

	if (!FPaths::FileExists(Filename))
	{
		UE_LOG(LogGFPakExporter, Log, TEXT("FAuroraExporterConfig::LoadJsonConfig: Json Configuration file '%s'%s not found."), *InJsonPath, *DebugAdjusted);
		return false;
	}

	FString InJsonString;
	if (!FFileHelper::LoadFileToString(InJsonString, *Filename))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraExporterConfig::LoadJsonConfig: Couldn't read file: '%s'%s"), *Filename, *DebugAdjusted);
		return false;
	}

	TSharedPtr<FJsonObject> JsonRoot = MakeShared<FJsonObject>();
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(InJsonString);
	if (!FJsonSerializer::Deserialize(JsonReader, JsonRoot))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraExporterConfig::LoadJsonConfig: Couldn't parse json data from file: '%s'%s"), *Filename, *DebugAdjusted);
		return false;
	}

	UE_LOG(LogGFPakExporter, Log, TEXT("FAuroraExporterConfig::LoadJsonConfig: Loading from file: '%s'%s"), *Filename, *DebugAdjusted);
	
	return LoadJsonConfig(JsonRoot);
}

bool FAuroraExporterConfig::SaveJsonConfig(const TSharedPtr<FJsonObject>& JsonObject) const
{
	if (!JsonObject)
	{
		return false;
	}
	
	FJsonObjectConverter::UStructToJsonObject(StaticStruct(), this, JsonObject.ToSharedRef(), 0, 0, nullptr, EJsonObjectConversionFlags::SkipStandardizeCase);
	return true;
}

bool FAuroraExporterConfig::SaveJsonConfig(const FString& InJsonPath) const
{
	if (InJsonPath.IsEmpty())
	{
		return false;
	}
	FString Filename = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*InJsonPath); //For Game builds to debug Sandbox File Paths
	
	const TSharedPtr<FJsonObject> JsonRoot = MakeShared<FJsonObject>();
	if (!SaveJsonConfig(JsonRoot))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraExporterConfig::SaveJsonConfig: Couldn't save the config to JSON: '%s'"), *Filename);
		return false;
	}
	
	FString OutJsonString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonString);
	if (!FJsonSerializer::Serialize(JsonRoot.ToSharedRef(), Writer))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraExporterConfig::SaveJsonConfig: Couldn't serialize the Json to file: '%s'"), *Filename);
		return false;
	}

	if (!FFileHelper::SaveStringToFile(OutJsonString, *Filename))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraExporterConfig::SaveJsonConfig: Couldn't save to file: '%s'"), *Filename);
		return false;
	}

	return true;
}

TOptional<FAuroraExporterConfig> FAuroraExporterConfig::FromPluginName(const FString& InPluginName)
{
	if (FPluginUtils::IsValidPluginName(InPluginName))
	{
		FAuroraExporterConfig Config;
		Config.DLCName = InPluginName;
		Config.Plugins.Add(FName{InPluginName});
		return Config;
	}
	return TOptional<FAuroraExporterConfig>{};
}
