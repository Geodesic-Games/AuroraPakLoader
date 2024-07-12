#include "PakRenderCommands.h"

#include "PakRenderStyle.h"

#define LOCTEXT_NAMESPACE "PakRender"

FPakRenderCommands::FPakRenderCommands()
	: TCommands<FPakRenderCommands>(TEXT("FPakRender")
	                                , LOCTEXT("PakRender", "Pak Render Window")
	                                , NAME_None
	                                , FPakRenderStyle::Get().GetStyleSetName()
	)
{
}

void FPakRenderCommands::RegisterCommands()
{
	UI_COMMAND(OpenRenderWindow, "Pak Render", "Open the Pak Render window", EUserInterfaceActionType::Button,
	           FInputChord());
}

#undef LOCTEXT_NAMESPACE
