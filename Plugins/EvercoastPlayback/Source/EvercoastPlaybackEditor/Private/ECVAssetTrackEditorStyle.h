#pragma once

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BORDER_BRUSH(RelativePath, ...) FSlateBorderBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define TTF_FONT(RelativePath, ...) FSlateFontInfo(RootToContentDir(RelativePath, TEXT(".ttf")), __VA_ARGS__)
#define OTF_FONT(RelativePath, ...) FSlateFontInfo(RootToContentDir(RelativePath, TEXT(".otf")), __VA_ARGS__)

class FECVAssetTrackEditorStyle
	: public FSlateStyleSet
{
public:
	FECVAssetTrackEditorStyle()
		: FSlateStyleSet("ECVAssetTrackEditorStyle")
	{
//		const FVector2D Icon10x10(10.0f, 10.0f);
		const FVector2D Icon16x16(16.0f, 16.0f);
//		const FVector2D Icon20x20(20.0f, 20.0f);
//		const FVector2D Icon24x24(24.0f, 24.0f);
//		const FVector2D Icon32x32(32.0f, 32.0f);
//		const FVector2D Icon40x40(40.0f, 40.0f);
		SetContentRoot(FPaths::ProjectPluginsDir() / TEXT("EvercoastPlayback/Content"));

		Set("Sequencer.Tracks.ECV", new IMAGE_BRUSH("Evercoast_16x", Icon16x16));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	static FECVAssetTrackEditorStyle& Get()
	{
		static FECVAssetTrackEditorStyle Inst;
		return Inst;
	}

	~FECVAssetTrackEditorStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}
};

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT
