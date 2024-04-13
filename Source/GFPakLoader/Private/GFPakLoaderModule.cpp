// Copyright GeoTech BV


#include "GFPakLoaderModule.h"

#include "GFPakLoaderLog.h"

DEFINE_LOG_CATEGORY(LogGFPakLoader);

#define LOCTEXT_NAMESPACE "FGFPakLoaderModule"

void FGFPakLoaderModule::StartupModule()
{
}

void FGFPakLoaderModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGFPakLoaderModule, GFPakLoader)
