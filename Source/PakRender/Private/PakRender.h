// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

class FPakRenderModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void PakRenderButtonClicked();

private:
	void RegisterMenus();

	TSharedRef<class SDockTab> OnSpawnPakRenderTab(const class FSpawnTabArgs& SpawnTabArgs);

	TSharedPtr <FUICommandList> PluginCommands;
};
