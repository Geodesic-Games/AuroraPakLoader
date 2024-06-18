// Copyright GeoTech BV

#include "GFPakLoaderEditorModule.h"

#include "ContentBrowserDataSubsystem.h"
#include "GFPakLoaderEditorLog.h"
#include "GFPakLoaderSubsystem.h"
#include "IContentBrowserDataModule.h"

DEFINE_LOG_CATEGORY(LogGFPakLoaderEditor);

#define LOCTEXT_NAMESPACE "FGFPakLoaderEditorModule"

const FString FGFPakLoaderEditorModule::GFPakLoaderVirtualPathPrefix{TEXT("/PakPlugins")}; 

void FGFPakLoaderEditorModule::StartupModule()
{
	
	FDelayedAutoRegisterHelper(EDelayedRegisterRunPhase::EndOfEngineInit,
		[]()
		{
			UContentBrowserDataSubsystem* ContentBrowserDataSubsystem = IContentBrowserDataModule::Get().GetSubsystem();
			if (ensure(ContentBrowserDataSubsystem))
			{
				ContentBrowserDataSubsystem->SetGenerateVirtualPathPrefixDelegate(
					FContentBrowserGenerateVirtualPathDelegate::CreateStatic(&FGFPakLoaderEditorModule::OnContentBrowserGenerateVirtualPathPrefix));
			}
		});
}

void FGFPakLoaderEditorModule::ShutdownModule()
{
}

void FGFPakLoaderEditorModule::OnContentBrowserGenerateVirtualPathPrefix(const FStringView InPath, FStringBuilderBase& OutPath)
{
	const FStringView MountPointStringView = FPathViews::GetMountPointNameFromPath(InPath);
	
	UGFPakLoaderSubsystem* Subsystem = UGFPakLoaderSubsystem::Get();
	if (IsValid(Subsystem)  && Subsystem->IsReady())
	{
		auto AppendPrefix = [&](const UGFPakPlugin* Plugin)
		{
			// Give a special virtual Path if the content is from a Pak Plugin : '/All/PakPlugins/<PluginName>/<MountPoint>/...'
			OutPath.Append(GFPakLoaderVirtualPathPrefix);
			OutPath.Append("/");
			OutPath.Append(Plugin->GetPluginName());
		};
		
		const TArray<UGFPakPlugin*> Plugins = Subsystem->GetPakPluginsWithStatusAtLeast(EGFPakLoaderStatus::Unmounted);
		for (const UGFPakPlugin* Plugin : Plugins)
		{
			// The main issue here is that some callbacks are triggered before the MountPoints are added to the plugin,
			// so the plugin currently has to force the content browser to refresh
			if (Plugin->GetStatus() == EGFPakLoaderStatus::Unmounted)
			{
				const FStringView PluginMountPointView = FPathViews::GetMountPointNameFromPath(Plugin->GetMountPointAboutToBeMounted().Key);
				if (PluginMountPointView == MountPointStringView)
				{
					AppendPrefix(Plugin);
					UE_LOG(LogGFPakLoaderEditor, VeryVerbose, TEXT(" OnContentBrowserGenerateVirtualPathPrefix:  Path  '%s' => Virtual Path  '%s'  as it is matching the MountPointAboutToBeMounted"), *FString{InPath}, *OutPath);
					return;
				}
			}
			
			if (Plugin->GetPluginName() == MountPointStringView)
			{
				AppendPrefix(Plugin);
				UE_LOG(LogGFPakLoaderEditor, VeryVerbose, TEXT(" OnContentBrowserGenerateVirtualPathPrefix:  Path  '%s' => Virtual Path  '%s'  as it is matching the PLUGIN NAME"), *FString{InPath}, *OutPath);
				return;
			}
			
			for (const TSharedPtr<FPluginMountPoint>& PluginMountPoint : Plugin->GetPakPluginMountPoints())
			{
				const FStringView PluginMountPointView = FPathViews::GetMountPointNameFromPath(PluginMountPoint->GetRootPath());
				if (PluginMountPoint->IsRegistered() && PluginMountPointView == MountPointStringView)
				{
					AppendPrefix(Plugin);
					UE_LOG(LogGFPakLoaderEditor, VeryVerbose, TEXT(" OnContentBrowserGenerateVirtualPathPrefix:  Path  '%s' => Virtual Path  '%s'  as it is matching the REGISTERED MOUNT POINT"), *FString{InPath}, *OutPath);
					return;
				}
			}
		}
	}
	
	// As per UContentBrowserDataSubsystem::ConvertInternalPathToVirtual;
	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(MountPointStringView))
	{
		if (Plugin->GetLoadedFrom() == EPluginLoadedFrom::Engine)
		{
			OutPath.Append(TEXT("/EngineData/Plugins"));
		}
		else
		{
			OutPath.Append(TEXT("/Plugins"));
		}

		const FPluginDescriptor& PluginDescriptor = Plugin->GetDescriptor();
		if (!PluginDescriptor.EditorCustomVirtualPath.IsEmpty())
		{
			int32 NumChars = PluginDescriptor.EditorCustomVirtualPath.Len();
			if (PluginDescriptor.EditorCustomVirtualPath.EndsWith(TEXT("/")))
			{
				--NumChars;
			}

			if (NumChars > 0)
			{
				if (!PluginDescriptor.EditorCustomVirtualPath.StartsWith(TEXT("/")))
				{
					OutPath.Append(TEXT("/"));
				}

				OutPath.Append(*PluginDescriptor.EditorCustomVirtualPath, NumChars);
			}
		}
	}
	else if (MountPointStringView.Equals(TEXT("Engine")))
	{
		OutPath.Append(TEXT("/EngineData"));
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGFPakLoaderEditorModule, GFPakLoaderEditor)
