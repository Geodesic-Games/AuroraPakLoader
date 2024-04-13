// Copyright GeoTech BV

#include "GFPakLoaderEditorModule.h"

#include "GFPakLoaderEditorLog.h"

DEFINE_LOG_CATEGORY(LogGFPakLoaderEditor);

#define LOCTEXT_NAMESPACE "FGFPakLoaderEditorModule"

void FGFPakLoaderEditorModule::StartupModule()
{
}

void FGFPakLoaderEditorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGFPakLoaderEditorModule, GFPakLoaderEditor)
