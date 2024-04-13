// Copyright GeoTech BV

#pragma once

#include "Framework/Commands/Commands.h"
#include "Styling/ISlateStyle.h"

#include "GFPakLoaderEditorStyle.h"

#define LOCTEXT_NAMESPACE "FGFPakLoaderEditorCommands"

class FGFPakLoaderEditorCommands : public TCommands<FGFPakLoaderEditorCommands>
{
public:
	FGFPakLoaderEditorCommands()
	    : TCommands<FGFPakLoaderEditorCommands>(
			TEXT("GFPakLoaderEditorCommands"), // Context name for fast lookup
			LOCTEXT("GFPakLoader", "GF Pak Loader"), // Localized context name for displaying
			NAME_None, // Parent
	        FGFPakLoaderEditorStyle::Get().GetStyleSetName() // Icon Style Set
		)
	{}

	//~Begin TCommand
	virtual void RegisterCommands() override;
	//~End TCommand
};

#undef LOCTEXT_NAMESPACE
