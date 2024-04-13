// Copyright GeoTech BV

#include "GFPakLoaderEditorStyle.h"

#include "Brushes/SlateColorBrush.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/StyleColors.h"
#include "Styling/SlateTypes.h"

FGFPakLoaderEditorStyle& FGFPakLoaderEditorStyle::Get()
{
	static FGFPakLoaderEditorStyle Instance;
	return Instance;
}

FGFPakLoaderEditorStyle::FGFPakLoaderEditorStyle()
	: FSlateStyleSet("GFPakLoaderEditorStyle")
{

	
	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FGFPakLoaderEditorStyle::~FGFPakLoaderEditorStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}
