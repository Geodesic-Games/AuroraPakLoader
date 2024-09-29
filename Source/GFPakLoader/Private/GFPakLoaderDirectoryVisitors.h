// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformFile.h"

struct FGFPakFilenameMap;

class FPakFileLister : public IPlatformFile::FDirectoryVisitor
{
public:
	/**
	 * 
	 * @param InMountPath The Mount Path returned by the IPakFile which will be prefixed to each path retrieved
	 * @param bInReturnMountedPaths If true, will return the Mounted Paths (ex: "/Engine/EngineMaterials/FlatNormal") instead of the raw paths (ex: "../../../Engine/Content/EngineMaterials/FlatNormal")
	 * @param bInIncludeExtension If true, the paths will contain extensions (like .uasset, .umap)
	 * @param bInIncludeNonAssets If true, all files will be returned, even the .ubulk and .uexp
	 * @param InTargetExtension If not empty, will only return files matching the given extension (need to include the ".") ex: ".umap"
	 */
	FPakFileLister(const FString& InMountPath, const bool bInReturnMountedPaths, const bool bInIncludeExtension, const bool bInIncludeNonAssets, const FString& InTargetExtension = FString())
		: MountPath(InMountPath)
		  , bReturnMountedPaths(bInReturnMountedPaths)
		  , bIncludeExtension(bInIncludeExtension)
		  , bIncludeNonAssets(bInIncludeNonAssets)
		  , TargetExtension(InTargetExtension)
	{
	}

	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override;

	FString MountPath;
	bool bReturnMountedPaths;
	bool bIncludeExtension;
	bool bIncludeNonAssets;
	FString TargetExtension;
	TArray<FString> Paths;
};


class FPakFilenameFinder : public IPlatformFile::FDirectoryVisitor
{
public:
	FPakFilenameFinder(const FString& InFilename)
		: Filename(InFilename)
	{
	}

	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override;

	FString Filename;
	FString Result;
};


class FPakContentFoldersFinder : public IPlatformFile::FDirectoryVisitor
{
public:
	FPakContentFoldersFinder(const FString& InMountPath)
		: MountPath(InMountPath)
	{
	}

	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override;

	FString MountPath;
	TArray<FString> ContentFolders;
};


class FPakGenerateFilenameMap : public IPlatformFile::FDirectoryVisitor
{
public:
	FPakGenerateFilenameMap(const FString& InOriginalMountPoint, const FString& InAdjustedMountPoint)
		: OriginalMountPoint(InOriginalMountPoint)
		  , AdjustedMountPoint(InAdjustedMountPoint)
	{
	}

	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override;

	FString OriginalMountPoint;
	FString AdjustedMountPoint;

	TMap<FName, TSharedPtr<const FGFPakFilenameMap>> PakFilenamesMap;
};
