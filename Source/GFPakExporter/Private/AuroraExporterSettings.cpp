// Copyright GeoTech BV


#include "AuroraExporterConfig.h"

#include "GFPakExporterLog.h"
#include "JsonObjectConverter.h"
#include "PluginUtils.h"


bool FAuroraExporterConfig::HasValidDLCName() const
{
	if (DLCName.IsEmpty())
	{
		return false;
	}
	FString PotentialPath = DLCName;
	FPaths::NormalizeDirectoryName(PotentialPath);
	FString Leaf = FPaths::GetPathLeaf(PotentialPath);
	return Leaf == DLCName && !DLCName.Contains(TEXT("."));
}

bool FAuroraExporterConfig::ShouldExportAsset(const FAssetData& AssetData) const
{
	if (Assets.Contains(AssetData.GetSoftObjectPath()))
	{
		return true;
	}
	
	return PackagePaths.ContainsByPredicate([&AssetData](const FAuroraDirectoryPath& ContentFolder)
	{
		return FPaths::IsUnderDirectory(AssetData.PackagePath.ToString(), ContentFolder.Path);
	});
}

FString FAuroraExporterConfig::GetDefaultDLCNameBasedOnContent(const FString& FallbackName) const
{
	TArray<FString> StartPath;
	bool bStartPathSet = false;

	auto SplitToArray = [](const FString& Path)
	{
		TArray<FString> PathArray;
		FString Str = Path;
		FString Left, Right;
		while (Str.Split(TEXT("/"), &Left, &Right))
		{
			PathArray.Add(Left);
			Str = MoveTemp(Right);
		}
		PathArray.Add(Str);
		return PathArray;
	};
	auto GetCommonPath = [](const TArray<FString>& Path1, const TArray<FString>& Path2)
	{
		TArray<FString> PathArray;
		int Max = FMath::Max(Path1.Num(), Path2.Num());
		for (int i = 0; i < Max; i++)
		{
			if (Path1[i] == Path2[i])
			{
				PathArray.Add(Path1[i]);
			}
			else
			{
				break;
			}
		}
		return PathArray;
	};
	auto IsPathEmpty = [] (const TArray<FString>& Path)
	{
		return Path.IsEmpty() || (Path.Num() == 1 && Path[0].IsEmpty());
	};
	
	for (const FAuroraDirectoryPath& PackagePath : PackagePaths)
	{
		TArray<FString> Path = SplitToArray(PackagePath.Path);
		if (!bStartPathSet)
		{
			StartPath = MoveTemp(Path);
			bStartPathSet = true;
		}
		else
		{
			StartPath = GetCommonPath(Path, StartPath);
			if (IsPathEmpty(StartPath))
			{
				break;
			}
		}
	}
	if (bStartPathSet && IsPathEmpty(StartPath))
	{
		return FallbackName;
	}

	for (const FSoftObjectPath& Asset : Assets)
	{
		TArray<FString> Path = SplitToArray(Asset.GetLongPackageName());
		if (!bStartPathSet)
		{
			StartPath = MoveTemp(Path);
			bStartPathSet = true;
		}
		else
		{
			StartPath = GetCommonPath(Path, StartPath);
			if (IsPathEmpty(StartPath))
			{
				break;
			}
		}
	}
	if (bStartPathSet && IsPathEmpty(StartPath))
	{
		return FallbackName;
	}
	return StartPath.Last();
}

TOptional<FAuroraExporterConfig> FAuroraExporterConfig::FromPluginName(const FString& InPluginName)
{
	if (FPluginUtils::IsValidPluginName(InPluginName))
	{
		FAuroraExporterConfig Config;
		Config.DLCName = InPluginName;
		Config.PackagePaths.Add(FAuroraDirectoryPath{TEXT("/") + InPluginName});
		return Config;
	}
	return TOptional<FAuroraExporterConfig>{};
}

bool FAuroraExporterSettings::LoadJsonSettings(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject)
	{
		return false;
	}
	
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), StaticStruct(), this);
}

bool FAuroraExporterSettings::LoadJsonSettings(const FString& InJsonPath)
{
	if (InJsonPath.IsEmpty())
	{
		return false;
	}
	FString Filename = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*InJsonPath); // For Game builds to debug Sandbox File Paths
	FString DebugAdjusted = InJsonPath == Filename ? FString{} : FString::Printf(TEXT(" (adjusted to '%s')"), *Filename);

	if (!FPaths::FileExists(Filename))
	{
		UE_LOG(LogGFPakExporter, Log, TEXT("FAuroraExporterSettings::LoadJsonConfig: Json Configuration file '%s'%s not found."), *InJsonPath, *DebugAdjusted);
		return false;
	}

	FString InJsonString;
	if (!FFileHelper::LoadFileToString(InJsonString, *Filename))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraExporterSettings::LoadJsonConfig: Couldn't read file: '%s'%s"), *Filename, *DebugAdjusted);
		return false;
	}

	TSharedPtr<FJsonObject> JsonRoot = MakeShared<FJsonObject>();
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(InJsonString);
	if (!FJsonSerializer::Deserialize(JsonReader, JsonRoot))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraExporterSettings::LoadJsonConfig: Couldn't parse json data from file: '%s'%s"), *Filename, *DebugAdjusted);
		return false;
	}

	UE_LOG(LogGFPakExporter, Log, TEXT("FAuroraExporterSettings::LoadJsonConfig: Loading from file: '%s'%s"), *Filename, *DebugAdjusted);
	SettingsFilePath.FilePath = InJsonPath;
	return LoadJsonSettings(JsonRoot);
}

bool FAuroraExporterSettings::SaveJsonSettings(const TSharedPtr<FJsonObject>& JsonObject) const
{
	if (!JsonObject)
	{
		return false;
	}
	
	FJsonObjectConverter::UStructToJsonObject(StaticStruct(), this, JsonObject.ToSharedRef(), 0, 0, nullptr, EJsonObjectConversionFlags::SkipStandardizeCase);
	return true;
}

bool FAuroraExporterSettings::SaveJsonSettings(const FString& InJsonPath) const
{
	if (InJsonPath.IsEmpty())
	{
		return false;
	}
	FString Filename = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*InJsonPath); //For Game builds to debug Sandbox File Paths
	
	const TSharedPtr<FJsonObject> JsonRoot = MakeShared<FJsonObject>();
	if (!SaveJsonSettings(JsonRoot))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraExporterSettings::SaveJsonConfig: Couldn't save the config to JSON: '%s'"), *Filename);
		return false;
	}
	
	FString OutJsonString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonString);
	if (!FJsonSerializer::Serialize(JsonRoot.ToSharedRef(), Writer))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraExporterSettings::SaveJsonConfig: Couldn't serialize the Json to file: '%s'"), *Filename);
		return false;
	}

	if (!FFileHelper::SaveStringToFile(OutJsonString, *Filename))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraExporterSettings::SaveJsonConfig: Couldn't save to file: '%s'"), *Filename);
		return false;
	}

	return true;
}
