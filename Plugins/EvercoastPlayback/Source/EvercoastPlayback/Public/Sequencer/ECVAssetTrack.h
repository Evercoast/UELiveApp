#pragma once

#include "MovieSceneNameableTrack.h"
#include "UObject/ObjectMacros.h"
#include "EvercoastECVAsset.h"
#include "Compilation/IMovieSceneTrackTemplateProducer.h"
#include "ECVAssetTrack.generated.h"


class UECVAssetTrackSection;

UCLASS()
class EVERCOASTPLAYBACK_API UECVAssetTrack : public UMovieSceneNameableTrack, public IMovieSceneTrackTemplateProducer
{
	GENERATED_BODY()
public:
	UECVAssetTrack(const FObjectInitializer& ObjectInitializer);

	UECVAssetTrackSection* AddNewECVAsset(UEvercoastECVAsset& InAsset, FFrameNumber InTime);

protected:
	// ~ Start of IMovieSceneTrackTemplateProducer interfaces
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;
	// ~ End of IMovieSceneTrackTemplateProducer interfaces

public:
	// ~ Start of UMovieSceneNameableTrack interfaces
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual bool IsEmpty() const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual void RemoveSectionAt(int32 SectionIndex) override;
	virtual bool SupportsMultipleRows() const override { return false; }
	// ~ Start of UMovieSceneNameableTrack interfaces

private:
	UPROPERTY()
	TArray<UMovieSceneSection*> ECVTrackSections;
};