#pragma once

class FPakRenderStyle final	: public FSlateStyleSet
{
public:
	static FPakRenderStyle& Get()
	{
		static FPakRenderStyle Instance;
		return Instance;
	}

	FPakRenderStyle();
	virtual ~FPakRenderStyle() override;
};
