#include "ECVAssetThumbnailSection.h"
#include "Sequencer/ECVAssetTrackSection.h"
#include "Sequencer/ECVAssetPersistentData.h"
#include "Sequencer/ECVAssetEvalTemplate.h"
#include "EvercoastECVAsset.h"
#include "SequencerSectionPainter.h"
#include "MovieSceneTimeHelpers.h"
#include "Compilation/MovieSceneCompiledDataManager.h"
#include "Evaluation/MovieSceneEvaluationTemplate.h"
#include "Fonts/FontMeasure.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
#include "TimeToPixel.h"
#else
#include "CommonMovieSceneTools.h"
#endif

#include "EvercoastStreamingReaderComp.h"


#define LOCTEXT_NAMESPACE "ECVAssetThumbnailSection"

FECVAssetThumbnailSection::FECVAssetThumbnailSection(UECVAssetTrackSection& InSection, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, TSharedPtr<ISequencer> InSequencer)
	: FThumbnailSection(InSequencer, InThumbnailPool, this, InSection)
	, SectionPtr(&InSection)
	, SequencerPtr(InSequencer)
{
}


FMargin FECVAssetThumbnailSection::GetContentPadding() const
{
	return FMargin(8.0f, 15.0f);
}

#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
float FECVAssetThumbnailSection::GetSectionHeight(const UE::Sequencer::FViewDensityInfo& ViewDensity) const
{
	return FThumbnailSection::GetSectionHeight(ViewDensity) + 2 * 9.0f; // make space for the film border
}
#else
float FECVAssetThumbnailSection::GetSectionHeight() const
{
	return FThumbnailSection::GetSectionHeight() + 2 * 9.0f; // make space for the film border
}
#endif


FText FECVAssetThumbnailSection::GetSectionTitle() const
{
	UECVAssetTrackSection* ECVSection = CastChecked<UECVAssetTrackSection>(Section);
	UEvercoastECVAsset* Asset = ECVSection->Asset;

	if (Asset == nullptr)
	{
		return LOCTEXT("NoSequence", "Empty");
	}

	return FText::FromString(Asset->GetFName().ToString());
}

void FECVAssetThumbnailSection::SetSingleTime(double GlobalTime)
{
	UECVAssetTrackSection* ECVSection = CastChecked<UECVAssetTrackSection>(Section);

	// TODO:
}

void FECVAssetThumbnailSection::AddReferencedObjects(FReferenceCollector& Collector)
{

}


int32 FECVAssetThumbnailSection::OnPaintSection(FSequencerSectionPainter& InPainter) const
{
	// draw background
	InPainter.LayerId = InPainter.PaintSectionBackground();

	FVector2D SectionSize = InPainter.SectionGeometry.GetLocalSize();
	FVector2D SectionTopLeft = InPainter.SectionGeometry.GetLocalPositionAtCoordinates(FVector2D(1.0f, 1.0f));
	FSlateClippingZone ClippingZone(InPainter.SectionClippingRect.InsetBy(FMargin(1.0f)));

	InPainter.DrawElements.PushClip(ClippingZone);
	{
		DrawFilmBorder(InPainter, SectionSize);
	}
	InPainter.DrawElements.PopClip();

	// draw thumbnails
	int32 LayerId = FThumbnailSection::OnPaintSection(InPainter) + 1;
	

	// draw loop overlays
	UEvercoastStreamingReaderComp* ECVReader = GetECVReader();

	if (ECVReader == nullptr)
	{
		return LayerId;
	}

	// draw overlays
	const FTimespan ECVDuration = FTimespan::FromSeconds( ECVReader->StreamingGetDuration() );

	if (ECVDuration.IsZero())
	{
		return LayerId;
	}

	UECVAssetTrackSection* ECVSection = SectionPtr.Get();
	if (!ECVSection)
	{
		return LayerId;
	}

	InPainter.DrawElements.PushClip(ClippingZone);
	{
		//DrawLoopIndicators(InPainter, ECVDuration, SectionSize);
		DrawCustomFrames(InPainter, ECVSection->StartFrameOffset, ECVDuration, SectionSize);
	}
	InPainter.DrawElements.PopClip();

	return LayerId;

}

UEvercoastStreamingReaderComp* FECVAssetThumbnailSection::GetECVReader() const
{
	// Otherwise, we'll need to find the reader through bound objects. This happens in spawnables
	UECVAssetTrackSection* ECVSection = SectionPtr.Get();
	if (ECVSection)
	{
		TSharedPtr<ISequencer> Sequencer = SequencerPtr.Pin();
		UMovieScene* MovieScene = ECVSection->GetTypedOuter<UMovieScene>();
		if (MovieScene && Sequencer)
		{
			TArray<FMovieSceneBinding> Bindings = MovieScene->GetBindings();
			for (const FMovieSceneBinding& Binding : Bindings)
			{
				// Check if this binding controls the section's track
				if (Binding.GetTracks().Contains(ECVSection->GetTypedOuter<UMovieSceneTrack>()))
				{
					// Query bound objects
					TArrayView<TWeakObjectPtr<UObject>> BoundObjects = Sequencer->FindBoundObjects(Binding.GetObjectGuid(), Sequencer->GetFocusedTemplateID());

					if (BoundObjects.Num() > 0)
					{
						UObject* SpawnedObject = BoundObjects[0].Get();
						if (SpawnedObject && SpawnedObject->IsA(AEvercoastVolcapActor::StaticClass()))
						{
							// Successfully found bound object, store it for later use
							AEvercoastVolcapActor* VolcapActor = CastChecked<AEvercoastVolcapActor>(SpawnedObject);
							return VolcapActor->Reader;
						}
					}
						
				}
			}
		}
	}

	return nullptr;

}

void FECVAssetThumbnailSection::DrawFilmBorder(FSequencerSectionPainter& InPainter, FVector2D SectionSize) const
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	static const FSlateBrush* FilmBorder = FAppStyle::GetBrush("Sequencer.Section.FilmBorder");
#else
	static const FSlateBrush* FilmBorder = FEditorStyle::GetBrush("Sequencer.Section.FilmBorder");
#endif

	// draw top film border
	FSlateDrawElement::MakeBox(
		InPainter.DrawElements,
		InPainter.LayerId++,
		InPainter.SectionGeometry.ToPaintGeometry(FVector2D(SectionSize.X - 2.0f, 7.0f), FSlateLayoutTransform(FVector2D(1.0f, 4.0f))),
		FilmBorder,
		InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect
	);

	// draw bottom film border
	FSlateDrawElement::MakeBox(
		InPainter.DrawElements,
		InPainter.LayerId++,
		InPainter.SectionGeometry.ToPaintGeometry(FVector2D(SectionSize.X - 2.0f, 7.0f), FSlateLayoutTransform(FVector2D(1.0f, SectionSize.Y - 11.0f))),
		FilmBorder,
		InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect
	);
}


void FECVAssetThumbnailSection::DrawCustomFrames(FSequencerSectionPainter& InPainter, FFrameNumber ECVAssetStartFrame, FTimespan ECVAssetDuration, FVector2D SectionSize) const
{
	static const FSlateBrush* GenericBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");

	UEvercoastStreamingReaderComp* ECVReader = GetECVReader();
	if (!ECVReader)
		return;

	//UECVAssetTrackSection* ECVSection = Cast<UECVAssetTrackSection>(Section);

	TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	//const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	const FSlateFontInfo FontInfo = FCoreStyle::Get().GetFontStyle(TEXT("NormalText"));
	const FLinearColor TextColor = FLinearColor::White;
	const FLinearColor BoxColor = FLinearColor::White;

	const FGeometry& Geometry = InPainter.SectionGeometry;
	const FSlateRect AbsoluteClipRect = Geometry.GetLayoutBoundingRect();
	const FVector2D LocalClipTopLeft = Geometry.AbsoluteToLocal(AbsoluteClipRect.GetTopLeft());
	const FVector2D LocalClipBottomRight = Geometry.AbsoluteToLocal(AbsoluteClipRect.GetBottomRight());
	const FSlateRect LocalClipRect(LocalClipTopLeft.X, LocalClipTopLeft.Y,
		LocalClipBottomRight.X, LocalClipBottomRight.Y);

	TRange<FFrameNumber> SectionRange = Section->GetRange();

	const FTimeToPixel& TimeToPixelConverter = InPainter.GetTimeConverter();

	FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
	FFrameRate DisplayRate = Section->GetTypedOuter<UMovieScene>()->GetDisplayRate();

	double SectionDuration = FFrameTime(UE::MovieScene::DiscreteSize(Section->GetRange())) / TickResolution;
	
	uint32_t FrameRate = ECVReader->StreamingGetCurrentFrameRate();
	float BoxWidth = TimeToPixelConverter.SecondsDeltaToPixel(1.0f / FrameRate);
	const FVector2D BoxSize(BoxWidth, 50.0f);

	int frameCount = 0;

	// convert tick frame -> tick time -> display time -> display frame
	FFrameNumber CurrentTickFrame = ECVAssetStartFrame;
	FFrameTime CurrentTickTime(CurrentTickFrame);
	FFrameTime CurrentDisplayTime = FFrameRate::TransformTime(CurrentTickTime, TickResolution, DisplayRate);
	FFrameNumber CurrentDisplayFrame = CurrentDisplayTime.FloorToFrame();

	// Frame number *without* ECVAssetStartFrame
	FFrameNumber SectionTickFrame = 0;
	FFrameTime SectionTickTime(SectionTickFrame);
	FFrameTime SectionDisplayTime = FFrameRate::TransformTime(SectionTickTime, TickResolution, DisplayRate);
	FFrameNumber SectionDisplayFrame = SectionDisplayTime.FloorToFrame();
	float DrawOffset = 0;

	while(DrawOffset < SectionSize.X)
	{
		// In-screen test
		FSlateRect QuadRect(DrawOffset, 5.0f, DrawOffset + BoxSize.X, 5.0f + BoxSize.Y);
		if (!FSlateRect::DoRectanglesIntersect(QuadRect, LocalClipRect))
		{
			break;
		}

		FVector2D BoxPosition(DrawOffset, 5.0f);

		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(BoxPosition, BoxSize),
			FCoreStyle::Get().GetBrush("FocusRectangle"),
			ESlateDrawEffect::None,
			BoxColor
		);

		FString FrameText = FString::Printf(TEXT("%d"), CurrentDisplayFrame.Value);
		FVector2D TextPosition = BoxPosition + FVector2D(2.0f, 2.0f);

		FVector2D TextSize = FontMeasureService->Measure(FrameText, FontInfo);


		FSlateDrawElement::MakeText(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(TextPosition, TextSize),
			FrameText,
			FontInfo,
			ESlateDrawEffect::None,
			TextColor
		);

		double clipTimestamp = CurrentTickTime / TickResolution;
		FString TimestampText = FString::Printf(TEXT("%.2f"), clipTimestamp);
		TextPosition = BoxPosition + FVector2D(2.0f, 52.0f);
		TextSize = FontMeasureService->Measure(TimestampText, FontInfo);

		FSlateDrawElement::MakeText(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(TextPosition, TextSize),
			TimestampText,
			FontInfo,
			ESlateDrawEffect::None,
			TextColor
		);


		// convert display frame -> display time -> tick time -> tick frame
		CurrentDisplayFrame++;
		CurrentDisplayTime = FFrameTime(CurrentDisplayFrame);
		CurrentTickTime = FFrameRate::TransformTime(CurrentDisplayTime, DisplayRate, TickResolution);
		CurrentTickFrame = CurrentTickTime.FloorToFrame();

		// check clip loop, if so reset counting
		if (clipTimestamp >= ECVAssetDuration.GetTotalSeconds())
		{
			
			CurrentTickFrame = 0;
			CurrentTickTime = FFrameTime(CurrentTickFrame);
			CurrentDisplayTime = FFrameRate::TransformTime(CurrentTickTime, TickResolution, DisplayRate);
			CurrentDisplayFrame = CurrentDisplayTime.FloorToFrame();


			// draw loop indicator
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				InPainter.LayerId++,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2

				// Need to explicitly specify transform order
				InPainter.SectionGeometry.ToPaintGeometry(
					FVector2D(1.0f, SectionSize.Y),  // size
					FSlateLayoutTransform(1.0f, // scale
						TransformPoint(1.0, // scale
							UE::Slate::CastToVector2f(FVector2D(DrawOffset, 0.0f) // offset
							)))),
#else
				// UE 4.27 - 5.1
				InPainter.SectionGeometry.ToPaintGeometry(FVector2D(BoxPosition.X + BoxSize.X, 0.0f), FVector2D(1.0f, SectionSize.Y)),
#endif

				GenericBrush,
				ESlateDrawEffect::None,
				FLinearColor::Gray
			);
		}

		// grid drawing frame/time update
		SectionDisplayFrame++;
		SectionDisplayTime = FFrameTime(SectionDisplayFrame);
		SectionTickTime = FFrameRate::TransformTime(SectionDisplayTime, DisplayRate, TickResolution);
		SectionTickFrame = SectionTickTime.FloorToFrame();
		
		
		DrawOffset = TimeToPixelConverter.SecondsDeltaToPixel(SectionDisplayTime / DisplayRate);
	}
	
}


#undef LOCTEXT_NAMESPACE