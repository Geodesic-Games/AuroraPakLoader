#include "PakRenderStyle.h"

#include "Styling/SlateStyleRegistry.h"

FPakRenderStyle::FPakRenderStyle()
	: FSlateStyleSet("PakRenderStyle")
{
	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FPakRenderStyle::~FPakRenderStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}
