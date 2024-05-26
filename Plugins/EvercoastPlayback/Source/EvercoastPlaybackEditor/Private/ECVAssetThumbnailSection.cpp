#include "ECVAssetThumbnailSection.h"
#include "Sequencer/ECVAssetTrackSection.h"
#include "Sequencer/ECVAssetPersistentData.h"
#include "Sequencer/ECVAssetEvalTemplate.h"
#include "EvercoastECVAsset.h"
#include "SequencerSectionPainter.h"
#include "MovieSceneTimeHelpers.h"
#include "Compilation/MovieSceneCompiledDataManager.h"
#include "Evaluation/MovieSceneEvaluationTemplate.h"
#include "CommonMovieSceneTools.h"

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


float FECVAssetThumbnailSection::GetSectionHeight() const
{
	return FThumbnailSection::GetSectionHeight() + 2 * 9.0f; // make space for the film border
}


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
	UEvercoastStreamingReaderComp* ECVReader = GetTemplateECVReader();

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

	InPainter.DrawElements.PushClip(ClippingZone);
	{
		DrawLoopIndicators(InPainter, ECVDuration, SectionSize);
	}
	InPainter.DrawElements.PopClip();

	return LayerId;

}

UEvercoastStreamingReaderComp* FECVAssetThumbnailSection::GetTemplateECVReader() const
{
	// locate the track that evaluates this section
	if (!SectionPtr.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<ISequencer> Sequencer = SequencerPtr.Pin();

	if (!Sequencer.IsValid())
	{
		return nullptr; // no movie scene player
	}

	// @todo: arodham: Test this and/or check dirty/compile?
	FMovieSceneRootEvaluationTemplateInstance& Instance = Sequencer->GetEvaluationTemplate();

	FMovieSceneSequenceID           SequenceId = Sequencer->GetFocusedTemplateID();
	UMovieSceneCompiledDataManager* CompiledDataManager = Instance.GetCompiledDataManager();
	UMovieSceneSequence* SubSequence = Instance.GetSequence(SequenceId);
	FMovieSceneCompiledDataID       CompiledDataID = CompiledDataManager->GetDataID(SubSequence);

	if (!CompiledDataID.IsValid())
	{
		return nullptr;
	}

	const FMovieSceneEvaluationTemplate* Template = CompiledDataManager->FindTrackTemplate(CompiledDataID);
	if (Template == nullptr)
	{
		return nullptr; // section template not found
	}

	auto OwnerTrack = Cast<UMovieSceneTrack>(SectionPtr->GetOuter());

	if (OwnerTrack == nullptr)
	{
		return nullptr; // media track not found
	}

	const FMovieSceneTrackIdentifier  TrackIdentifier = Template->GetLedger().FindTrackIdentifier(OwnerTrack->GetSignature());
	const FMovieSceneEvaluationTrack* EvaluationTrack = Template->FindTrack(TrackIdentifier);

	if (EvaluationTrack == nullptr)
	{
		return nullptr; // evaluation track not found
	}

	FECVAssetPersistentData* ECVAssetData = nullptr;

	// find the persistent data of the section being drawn
	TArrayView<const FMovieSceneEvalTemplatePtr> Children = EvaluationTrack->GetChildTemplates();
	FPersistentEvaluationData PersistentData(*Sequencer.Get());

	for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
	{
		if (Children[ChildIndex]->GetSourceSection() == SectionPtr)
		{
			FMovieSceneEvaluationKey SectionKey(SequenceId, TrackIdentifier, ChildIndex);
			PersistentData.SetSectionKey(SectionKey);
			ECVAssetData = PersistentData.FindSectionData<FECVAssetPersistentData>();

			break;
		}
	}

	// get the template's media player
	if (ECVAssetData == nullptr)
	{
		return nullptr; // section persistent data not found
	}

	return ECVAssetData->GetEvercoastReader();
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


void FECVAssetThumbnailSection::DrawLoopIndicators(FSequencerSectionPainter& InPainter, FTimespan AssetDuration, FVector2D SectionSize) const
{
	static const FSlateBrush* GenericBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");

	UECVAssetTrackSection* ECVSection = Cast<UECVAssetTrackSection>(Section);

	const FTimeToPixel& TimeToPixelConverter = InPainter.GetTimeConverter();

	FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
	double SectionDuration = FFrameTime(UE::MovieScene::DiscreteSize(Section->GetRange())) / TickResolution;
	float MediaSizeX = AssetDuration.GetTotalSeconds() * SectionSize.X / SectionDuration;
	// Use SecondsDeltaToPixel()
	float DrawOffset = MediaSizeX - TimeToPixelConverter.SecondsDeltaToPixel(TickResolution.AsSeconds(ECVSection->StartFrameOffset));

	while (DrawOffset < SectionSize.X)
	{
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
			InPainter.SectionGeometry.ToPaintGeometry(FVector2D(DrawOffset, 0.0f), FVector2D(1.0f, SectionSize.Y)),
#endif
			
			GenericBrush,
			ESlateDrawEffect::None,
			FLinearColor::Gray
		);

		DrawOffset += MediaSizeX;
	}
}


#undef LOCTEXT_NAMESPACE