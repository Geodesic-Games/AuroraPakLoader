// Copyright Epic Games, Inc. All Rights Reserved.

#include "PakRender.h"

#include "UI/PakRenderCommands.h"
#include "UI/SPakRenderWindow.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FPakRenderModule"

void FPakRenderModule::StartupModule()
{
	FPakRenderCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FPakRenderCommands::Get().OpenRenderWindow,
		FExecuteAction::CreateRaw(this, &FPakRenderModule::PakRenderButtonClicked)
	);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPakRenderModule::RegisterMenus));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner("PakRenderTab", FOnSpawnTab::CreateRaw(this, &FPakRenderModule::OnSpawnPakRenderTab))
		.SetDisplayName(LOCTEXT("PakRenderTabTitle", "Pak Render"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FPakRenderModule::ShutdownModule()
{
}

void FPakRenderModule::PakRenderButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId("PakRenderTab"));
}

void FPakRenderModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
	FToolMenuSection& Section = Menu->FindOrAddSection("Log");
	Section.AddMenuEntryWithCommandList(FPakRenderCommands::Get().OpenRenderWindow, PluginCommands);
}

TSharedRef<SDockTab> FPakRenderModule::OnSpawnPakRenderTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
	.TabRole(NomadTab)
	.Label(LOCTEXT("PakRenderTabTitle", "Pak Render"))
	[
		SNew(SPakRenderWindow)
	];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPakRenderModule, PakRender)
