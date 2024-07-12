#pragma once

class FPakRenderCommands : public TCommands<FPakRenderCommands>
{
public:
	FPakRenderCommands();

	// Begin TCommands interface
	virtual void RegisterCommands() override;
	// End TCommands interface

	TSharedPtr<FUICommandInfo> OpenRenderWindow;
};
