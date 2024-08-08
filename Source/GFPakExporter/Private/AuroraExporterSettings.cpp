// Copyright GeoTech BV


#include "AuroraExporterSettings.h"

#include "GFPakExporterLog.h"
#include "JsonObjectConverter.h"
#include "PluginUtils.h"
#include "Algo/MaxElement.h"


// -- FAuroraContentDLCExporterConfig

bool FAuroraContentDLCExporterConfig::HasValidDLCName() const
{
	if (DLCName.IsEmpty())
	{
		return false;
	}
	FString PotentialPath = DLCName;
	FPaths::NormalizeDirectoryName(PotentialPath);
	const FString Leaf = FPaths::GetPathLeaf(PotentialPath);
	return Leaf == DLCName && !DLCName.Contains(TEXT("."));
}

bool FAuroraContentDLCExporterConfig::ShouldExportAsset(const FAssetData& AssetData) const
{
	const FName& PackageName = AssetData.PackageName;

	// Some packages (like blueprints) might contains multiple assets, but we need to make sure all the assets from the package would be exported
	if (Assets.ContainsByPredicate([&PackageName](const FSoftObjectPath& Asset)
		{
			return Asset.GetLongPackageFName() == PackageName;
		}))
	{
		return true;
	}
	
	return PackagePaths.ContainsByPredicate([PackageNameStr = PackageName.ToString()](const FAuroraDirectoryPath& ContentFolder)
	{
		return !ContentFolder.Path.IsEmpty() && FPaths::IsUnderDirectory(PackageNameStr, ContentFolder.Path);
	});
}

FString FAuroraContentDLCExporterConfig::GetDefaultDLCNameBasedOnContent(const FString& FallbackName) const
{
	// Here we are trying the get the common path between all the assets, and return the name of the common folder containing all these assets
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
	return StartPath.IsEmpty() ? FallbackName : StartPath.Last();
}

TOptional<FAuroraContentDLCExporterConfig> FAuroraContentDLCExporterConfig::FromPluginName(const FString& InPluginName)
{
	if (FPluginUtils::IsValidPluginName(InPluginName))
	{
		FAuroraContentDLCExporterConfig Config;
		Config.DLCName = InPluginName;
		Config.PackagePaths.Add(FAuroraDirectoryPath{TEXT("/") + InPluginName});
		return Config;
	}
	return TOptional<FAuroraContentDLCExporterConfig>{};
}


// --- FAuroraBaseGameExporterConfig

FAuroraBaseGameExporterConfig::EAssetExportRule FAuroraBaseGameExporterConfig::GetAssetExportRule(FName PackageName) const
{
	// Firstly, if we are specifically referencing an asset, we can return the ExportRule directly
	if (AssetsToInclude.ContainsByPredicate([&PackageName](const FSoftObjectPath& Asset)
		{
			return Asset.GetLongPackageFName() == PackageName;
		}))
	{
		return EAssetExportRule::Include;
	}

	if (AssetsToExclude.ContainsByPredicate([&PackageName](const FSoftObjectPath& Asset)
		{
			return Asset.GetLongPackageFName() == PackageName;
		}))
	{
		return EAssetExportRule::Exclude;
	}

	// If not, an asset might be indirectly referenced by both the PackagePathsTo*Include* and PackagePathsTo*Exclude*
	// Ex: Include: /Game/Maps   Exclude: /Game/Maps/Folder/
	// If that is the case, the most specific one should win, which is also the longest path. If they are the same, the asset will be included
	const FAuroraDirectoryPath* MostSpecificInclude = Algo::MaxElementBy(PackagePathsToInclude, [&PackageName](const FAuroraDirectoryPath& ContentFolder)
	{
		return !ContentFolder.Path.IsEmpty() && FPaths::IsUnderDirectory(PackageName.ToString(), ContentFolder.Path) ?
			ContentFolder.Path.Len() : 0;
	});
	const int MostSpecificIncludeLength = MostSpecificInclude ? MostSpecificInclude->Path.Len() : 0;
	
	const FAuroraDirectoryPath* MostSpecificExclude = Algo::MaxElementBy(PackagePathsToExclude, [&PackageName](const FAuroraDirectoryPath& ContentFolder)
	{
		return !ContentFolder.Path.IsEmpty() && FPaths::IsUnderDirectory(PackageName.ToString(), ContentFolder.Path) ?
			ContentFolder.Path.Len() : 0;
	});
	const int MostSpecificExcludeLength = MostSpecificExclude ? MostSpecificExclude->Path.Len() : 0;
	
	if (MostSpecificIncludeLength > 0 && MostSpecificIncludeLength >= MostSpecificExcludeLength)
	{
		return EAssetExportRule::Include;
	}
	if (MostSpecificExcludeLength > 0)
	{
		return EAssetExportRule::Exclude;
	}
	
	return EAssetExportRule::Unknown;
}


// --- FAuroraContentDLCExporterSettings

bool FAuroraContentDLCExporterSettings::LoadJsonSettings(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject)
	{
		return false;
	}
	
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), StaticStruct(), this);
}

bool FAuroraContentDLCExporterSettings::LoadJsonSettings(const FString& InJsonPath)
{
	if (InJsonPath.IsEmpty())
	{
		return false;
	}
	const FString Filename = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*InJsonPath); // For Game builds to debug Sandbox File Paths
	const FString DebugAdjusted = InJsonPath == Filename ? FString{} : FString::Printf(TEXT(" (adjusted to '%s')"), *Filename);

	if (!FPaths::FileExists(Filename))
	{
		UE_LOG(LogGFPakExporter, Log, TEXT("FAuroraContentDLCExporterSettings::LoadJsonConfig: Json Configuration file '%s'%s not found."), *InJsonPath, *DebugAdjusted);
		return false;
	}

	FString InJsonString;
	if (!FFileHelper::LoadFileToString(InJsonString, *Filename))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraContentDLCExporterSettings::LoadJsonConfig: Couldn't read file: '%s'%s"), *Filename, *DebugAdjusted);
		return false;
	}

	TSharedPtr<FJsonObject> JsonRoot = MakeShared<FJsonObject>();
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(InJsonString);
	if (!FJsonSerializer::Deserialize(JsonReader, JsonRoot))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraContentDLCExporterSettings::LoadJsonConfig: Couldn't parse json data from file: '%s'%s"), *Filename, *DebugAdjusted);
		return false;
	}

	UE_LOG(LogGFPakExporter, Verbose, TEXT("FAuroraContentDLCExporterSettings::LoadJsonConfig: Loading from file: '%s'%s"), *Filename, *DebugAdjusted);
	SettingsFilePath.FilePath = InJsonPath;
	return LoadJsonSettings(JsonRoot);
}

bool FAuroraContentDLCExporterSettings::SaveJsonSettings(const TSharedPtr<FJsonObject>& JsonObject) const
{
	if (!JsonObject)
	{
		return false;
	}
	
	FJsonObjectConverter::UStructToJsonObject(StaticStruct(), this, JsonObject.ToSharedRef(), 0, 0, nullptr, EJsonObjectConversionFlags::SkipStandardizeCase);
	return true;
}

bool FAuroraContentDLCExporterSettings::SaveJsonSettings(const FString& InJsonPath) const
{
	if (InJsonPath.IsEmpty())
	{
		return false;
	}
	const FString Filename = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*InJsonPath); //For Game builds to debug Sandbox File Paths
	
	const TSharedPtr<FJsonObject> JsonRoot = MakeShared<FJsonObject>();
	if (!SaveJsonSettings(JsonRoot))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraContentDLCExporterSettings::SaveJsonConfig: Couldn't save the config to JSON: '%s'"), *Filename);
		return false;
	}
	
	FString OutJsonString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonString);
	if (!FJsonSerializer::Serialize(JsonRoot.ToSharedRef(), Writer))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraContentDLCExporterSettings::SaveJsonConfig: Couldn't serialize the Json to file: '%s'"), *Filename);
		return false;
	}

	if (!FFileHelper::SaveStringToFile(OutJsonString, *Filename))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraContentDLCExporterSettings::SaveJsonConfig: Couldn't save to file: '%s'"), *Filename);
		return false;
	}

	return true;
}


// --- FAuroraBaseGameExporterSettings

bool FAuroraBaseGameExporterSettings::LoadJsonSettings(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject)
	{
		return false;
	}
	
	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), StaticStruct(), this);
}

bool FAuroraBaseGameExporterSettings::LoadJsonSettings(const FString& InJsonPath)
{
	if (InJsonPath.IsEmpty())
	{
		return false;
	}
	const FString Filename = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*InJsonPath); // For Game builds to debug Sandbox File Paths
	const FString DebugAdjusted = InJsonPath == Filename ? FString{} : FString::Printf(TEXT(" (adjusted to '%s')"), *Filename);

	if (!FPaths::FileExists(Filename))
	{
		UE_LOG(LogGFPakExporter, Log, TEXT("FAuroraBaseGameExporterSettings::LoadJsonConfig: Json Configuration file '%s'%s not found."), *InJsonPath, *DebugAdjusted);
		return false;
	}

	FString InJsonString;
	if (!FFileHelper::LoadFileToString(InJsonString, *Filename))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraBaseGameExporterSettings::LoadJsonConfig: Couldn't read file: '%s'%s"), *Filename, *DebugAdjusted);
		return false;
	}

	TSharedPtr<FJsonObject> JsonRoot = MakeShared<FJsonObject>();
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(InJsonString);
	if (!FJsonSerializer::Deserialize(JsonReader, JsonRoot))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraBaseGameExporterSettings::LoadJsonConfig: Couldn't parse json data from file: '%s'%s"), *Filename, *DebugAdjusted);
		return false;
	}

	UE_LOG(LogGFPakExporter, Verbose, TEXT("FAuroraBaseGameExporterSettings::LoadJsonConfig: Loading from file: '%s'%s"), *Filename, *DebugAdjusted);
	SettingsFilePath.FilePath = InJsonPath;
	return LoadJsonSettings(JsonRoot);
}

bool FAuroraBaseGameExporterSettings::SaveJsonSettings(const TSharedPtr<FJsonObject>& JsonObject) const
{
	if (!JsonObject)
	{
		return false;
	}
	
	FJsonObjectConverter::UStructToJsonObject(StaticStruct(), this, JsonObject.ToSharedRef(), 0, 0, nullptr, EJsonObjectConversionFlags::SkipStandardizeCase);
	return true;
}

bool FAuroraBaseGameExporterSettings::SaveJsonSettings(const FString& InJsonPath) const
{
	if (InJsonPath.IsEmpty())
	{
		return false;
	}
	const FString Filename = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*InJsonPath); //For Game builds to debug Sandbox File Paths
	
	const TSharedPtr<FJsonObject> JsonRoot = MakeShared<FJsonObject>();
	if (!SaveJsonSettings(JsonRoot))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraBaseGameExporterSettings::SaveJsonConfig: Couldn't save the config to JSON: '%s'"), *Filename);
		return false;
	}
	
	FString OutJsonString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonString);
	if (!FJsonSerializer::Serialize(JsonRoot.ToSharedRef(), Writer))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraBaseGameExporterSettings::SaveJsonConfig: Couldn't serialize the Json to file: '%s'"), *Filename);
		return false;
	}

	if (!FFileHelper::SaveStringToFile(OutJsonString, *Filename))
	{
		UE_LOG(LogGFPakExporter, Error, TEXT("FAuroraBaseGameExporterSettings::SaveJsonConfig: Couldn't save to file: '%s'"), *Filename);
		return false;
	}

	return true;
}
