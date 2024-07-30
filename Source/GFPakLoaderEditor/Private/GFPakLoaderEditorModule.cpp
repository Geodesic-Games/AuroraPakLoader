// Copyright GeoTech BV

#include "GFPakLoaderEditorModule.h"

#include "ContentBrowserDataSubsystem.h"
#include "GFPakLoaderEditorLog.h"
#include "GFPakLoaderSubsystem.h"
#include "IContentBrowserDataModule.h"

DEFINE_LOG_CATEGORY(LogGFPakLoaderEditor);

#define LOCTEXT_NAMESPACE "FGFPakLoaderEditorModule"

const FString FGFPakLoaderEditorModule::GFPakLoaderVirtualPathPrefix{TEXT("/DLC Paks")}; 

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

		UGFPakPlugin* Plugin = nullptr;
		FString DebugReason;
		Subsystem->EnumeratePakPluginsWithStatus<UGFPakLoaderSubsystem::EComparison::GreaterOrEqual>(EGFPakLoaderStatus::Unmounted, [&MountPointStringView, &Plugin, &DebugReason](UGFPakPlugin* PakPlugin)
		{
			// The main issue here is that some callbacks are triggered before the MountPoints are added to the plugin,
			// so the plugin currently has to force the content browser to refresh
			if (PakPlugin->GetStatus() == EGFPakLoaderStatus::Unmounted) 
			{
				//todo: Minor bug, when a Plugin with the same name as a Pak plugin exists, this will end up showing the Plugin content in the PakPlugins folder, even though the PakPlugin will not be able to Mount
				// This should be fixed with a "Mounting" state
				const FStringView PluginMountPointView = FPathViews::GetMountPointNameFromPath(PakPlugin->GetMountPointAboutToBeMounted().Key);
				if (PluginMountPointView == MountPointStringView)
				{
					Plugin = PakPlugin;
					DebugReason = "MountPointAboutToBeMounted";
					return UGFPakLoaderSubsystem::EForEachResult::Break;
				}
			}
			
			if (PakPlugin->GetPluginName() == MountPointStringView)
			{
				Plugin = PakPlugin;
				DebugReason = "PLUGIN NAME";
				return UGFPakLoaderSubsystem::EForEachResult::Break;
			}
			
			for (const TSharedPtr<FPluginMountPoint>& PluginMountPoint : PakPlugin->GetPakPluginMountPoints())
			{
				const FStringView PluginMountPointView = FPathViews::GetMountPointNameFromPath(PluginMountPoint->GetRootPath());
				if (PluginMountPoint->IsRegistered() && PluginMountPointView == MountPointStringView)
				{
					Plugin = PakPlugin;
					DebugReason = "REGISTERED MOUNT POINT";
					return UGFPakLoaderSubsystem::EForEachResult::Break;
				}
			}
			return UGFPakLoaderSubsystem::EForEachResult::Continue;
		});

		if (Plugin)
		{
			AppendPrefix(Plugin);
            UE_LOG(LogGFPakLoaderEditor, VeryVerbose, TEXT(" OnContentBrowserGenerateVirtualPathPrefix:  Path  '%s' => Virtual Path  '%s'  as it is matching the %s"), *FString{InPath}, *OutPath, *DebugReason);
			return;
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
