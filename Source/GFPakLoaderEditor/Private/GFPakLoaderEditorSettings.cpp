// Copyright GeoTech BV

#include "GFPakLoaderEditorSettings.h"

UGFPakLoaderEditorSettings::UGFPakLoaderEditorSettings()
    : StagingDirectoryName(TEXT("_TEMP_STAGING_"))
    , bCompressPaks(true)
{
	BuildOutputDirectory.Path = TEXT("C:/UE-BuildOutput");
}
