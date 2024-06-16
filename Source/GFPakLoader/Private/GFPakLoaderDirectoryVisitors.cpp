// Copyright GeoTech BV


#include "GFPakLoaderDirectoryVisitors.h"

#include "GFPakLoaderLog.h"
#include "GFPakPlugin.h"


bool FPakFileLister::Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
{
	if (!bIsDirectory)
	{
		const FString Fullname = MountPath + FilenameOrDirectory;
		if (bReturnMountedPaths)
		{
			FString LocalPath; // ex: "../../../Engine/Content/EngineMaterials/FlatNormal"
			FString PackageName; //ex: "/Engine/EngineMaterials/FlatNormal"
			FString Extension; //ex: ".uasset" or ".umap"
			if (FPackageName::TryConvertToMountedPath(Fullname, &LocalPath, &PackageName, nullptr, nullptr, &Extension))
			{
				if (bIncludeNonAssets || Extension == TEXT(".uasset") || Extension == TEXT(".umap"))
				{
					if (TargetExtension.IsEmpty() || Extension == TargetExtension)
					{
						Paths.Add(bIncludeExtension ? PackageName + Extension : PackageName);
					}
				}
			}
		}
		else
		{
			const FString Extension = FPaths::GetExtension(Fullname, true);
			if (bIncludeNonAssets || Extension == TEXT(".uasset") || Extension == TEXT(".umap"))
			{
				if (TargetExtension.IsEmpty() || Extension == TargetExtension)
				{
					Paths.Add(bIncludeExtension ? Fullname : FPaths::GetBaseFilename(Fullname, false));
				}
			}
		}
	}
	return true;
}

bool FPakFilenameFinder::Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
{
	UE_LOG(LogGFPakLoader, Verbose, TEXT("  - Visit: '%s' => '%s'"), FilenameOrDirectory, *FPaths::GetCleanFilename(FilenameOrDirectory)); //todo: remove
	if (!bIsDirectory && Filename == FPaths::GetCleanFilename(FilenameOrDirectory))
	{
		Result = FilenameOrDirectory;
		return false;
	}
	return true;
}

bool FPakContentFoldersFinder::Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
{
	if (!bIsDirectory)
	{
		const FString Fullname = MountPath + FilenameOrDirectory;
		FString PreContent;
		if (Fullname.Split(TEXT("/Content/"), &PreContent, nullptr))
		{
			ContentFolders.AddUnique(PreContent + TEXT("/Content/"));
		}
	}
	return true;
}

bool FPakGenerateFilenameMap::Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
{
	if (bIsDirectory)
	{
		return false;
	}
		
	FGFPakFilenameMap PakFilename = FGFPakFilenameMap::FromFilenameAndMountPoints(OriginalMountPoint, AdjustedMountPoint, FilenameOrDirectory);
	const FName Path{PakFilename.AdjustedFullFilename};
	if (ensure(!Path.IsNone() && !PakFilenamesMap.Contains(Path)))
	{
		TSharedRef<FGFPakFilenameMap> SharedPakFilename = MakeShared<FGFPakFilenameMap>(PakFilename);
		PakFilenamesMap.Add(Path, SharedPakFilename);
		if (!SharedPakFilename->LocalBaseFilename.IsNone())
		{
			PakFilenamesMap.Add(SharedPakFilename->LocalBaseFilename, SharedPakFilename);
		}
	}
		
	return false;
}


