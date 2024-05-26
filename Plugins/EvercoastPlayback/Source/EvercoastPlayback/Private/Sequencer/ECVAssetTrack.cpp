#include "Sequencer/ECVAssetTrack.h"
#include "Sequencer/ECVAssetTrackSection.h"
#include "Sequencer/ECVAssetEvalTemplate.h"
#include "MovieScene.h"

UECVAssetTrack::UECVAssetTrack(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{

}

UECVAssetTrackSection* UECVAssetTrack::AddNewECVAsset(UEvercoastECVAsset& InAsset, FFrameNumber InTime)
{
	const float DefaultSectionDuration = 10.0f;
	FFrameRate TickResolution = GetTypedOuter<UMovieScene>()->GetTickResolution();
	FFrameTime DurationToUse = DefaultSectionDuration * TickResolution;

	UECVAssetTrackSection* NewSection = Cast<UECVAssetTrackSection>(CreateNewSection());
	NewSection->InitialPlacement(ECVTrackSections, InTime, DurationToUse.FrameNumber.Value, false);
	NewSection->Asset = &InAsset;

	AddSection(*NewSection);
	return NewSection;
}

FMovieSceneEvalTemplatePtr UECVAssetTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FECVAssetEvalTemplate(*CastChecked<const UECVAssetTrackSection>(&InSection), *this);
}


bool UECVAssetTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UECVAssetTrackSection::StaticClass();
}

UMovieSceneSection* UECVAssetTrack::CreateNewSection()
{
	UECVAssetTrackSection* NewSection = NewObject<UECVAssetTrackSection>(this, NAME_None, RF_Transactional);

	// Probably want to do some initialisation work for the section here
	return NewSection;
}

void UECVAssetTrack::AddSection(UMovieSceneSection& Section)
{
	ECVTrackSections.Add(&Section);
}

bool UECVAssetTrack::HasSection(const UMovieSceneSection& Section) const
{
	return ECVTrackSections.Contains(&Section);
}

bool UECVAssetTrack::IsEmpty() const
{
	return ECVTrackSections.Num() == 0;
}

const TArray<UMovieSceneSection*>& UECVAssetTrack::GetAllSections() const
{
	return ECVTrackSections;
}

void UECVAssetTrack::RemoveSection(UMovieSceneSection& Section)
{
	ECVTrackSections.Remove(&Section);
}

void UECVAssetTrack::RemoveSectionAt(int32 SectionIndex)
{
	ECVTrackSections.RemoveAt(SectionIndex);
}

