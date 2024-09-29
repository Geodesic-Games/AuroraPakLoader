// Copyright GeoTech BV

#pragma once

#include "CoreTypes.h"
#include "AuroraSaveFilePath.generated.h"

/** Structure for file paths that are displayed in the editor with a picker UI. */
USTRUCT(BlueprintType)
struct FAuroraSaveFilePath
{
	GENERATED_BODY()
	/**
	 * The path to the file.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=FAuroraSaveFilePath)
	FString FilePath;
};

/** Structure for directory paths that are displayed in the editor with a picker UI. */
USTRUCT(BlueprintType)
struct FAuroraDirectoryPath
{
	GENERATED_BODY()
	/**
	 * The path to the directory.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=FAuroraDirectoryPath)
	FString Path;
};