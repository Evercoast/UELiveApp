#include "Sequencer/ECVAssetTrackSection.h"


namespace
{
	FFrameNumber GetStartOffsetAtTrimTime(FQualifiedFrameTime TrimTime, FFrameNumber StartOffset, FFrameNumber StartFrame)
	{
		return StartOffset + TrimTime.Time.FrameNumber - StartFrame;
	}
}


UECVAssetTrackSection::UECVAssetTrackSection(const FObjectInitializer& InInitializer) :
	Super(InInitializer),
	Asset(nullptr),
	//ReaderRendererActor(nullptr),
	StartFrameOffset(0)
{
	// When section no longer evaluated, restore the states
	EvalOptions.CompletionMode = EMovieSceneCompletionMode::RestoreState;
}

// Same as UMovieSceneMediaSection::PostInitProperties
void UECVAssetTrackSection::PostInitProperties()
{
	Super::PostInitProperties();

	UMovieScene* Outer = GetTypedOuter<UMovieScene>();
	FFrameRate TickResolution = Outer ? Outer->GetTickResolution() : FFrameRate(15, 1);

	// media tracks have some preroll by default to precache frames
	SetPreRollFrames((0.5 * TickResolution).RoundToFrame().Value);
}

void UECVAssetTrackSection::TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft, bool bDeleteKeys)
{
	if (TryModify())
	{
		if (bTrimLeft)
		{
			StartFrameOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(TrimTime, StartFrameOffset, GetInclusiveStartFrame()) : 0;
		}

		Super::TrimSection(TrimTime, bTrimLeft, bDeleteKeys);
	}
}

UMovieSceneSection* UECVAssetTrackSection::SplitSection(FQualifiedFrameTime SplitTime, bool bDeleteKeys)
{
	const FFrameNumber InitialStartFrameOffset = StartFrameOffset;

	// Calc new offset after splitting
	const FFrameNumber NewOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(SplitTime, StartFrameOffset, GetInclusiveStartFrame()) : 0;

	UMovieSceneSection* NewSection = Super::SplitSection(SplitTime, bDeleteKeys);
	if (NewSection != nullptr)
	{
		UECVAssetTrackSection* NewECVSection = Cast<UECVAssetTrackSection>(NewSection);
		NewECVSection->StartFrameOffset = NewOffset;
	}

	// Restore original offset (might be) modified by splitting
	StartFrameOffset = InitialStartFrameOffset;

	return NewSection;
}

TOptional<FFrameTime> UECVAssetTrackSection::GetOffsetTime() const
{
	return TOptional<FFrameTime>(StartFrameOffset);
}

#if WITH_EDITOR
void UECVAssetTrackSection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif