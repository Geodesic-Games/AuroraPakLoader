// Copyright GeoTech BV

#pragma once

#include "Styling/SlateStyle.h"

class FGFPakLoaderEditorStyle final : public FSlateStyleSet
{
public:
	static FGFPakLoaderEditorStyle& Get();

private:
	FGFPakLoaderEditorStyle();
	~FGFPakLoaderEditorStyle();
};
