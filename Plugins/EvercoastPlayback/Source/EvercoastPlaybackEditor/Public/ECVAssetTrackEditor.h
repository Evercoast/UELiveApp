#pragma once

#include "CoreMinimal.h"
#include "ISequencer.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "MovieSceneTrackEditor.h"
#include "AnimatedPropertyKey.h"

class UECVAssetTrack;
class FTrackEditorThumbnailPool;

class FECVAssetTrackEditor : public FMovieSceneTrackEditor
{
public:


	FECVAssetTrackEditor(TSharedRef<ISequencer> InSequencer);


	/**
	 * Create a new media track editor instance.
	 *
	 * @param OwningSequencer The sequencer object that will own the track editor.
	 * @return The new track editor.
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

	/**
	 * Get the list of all property types that this track editor animates.
	 *
	 * @return List of animated properties.
	 */
	static TArray<FAnimatedPropertyKey, TInlineAllocator<1>> GetAnimatedPropertyTypes();

public:
	// ~Start of FMovieSceneTrackEditor interfaces
	virtual UMovieSceneTrack* AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName) override;
	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override;
	// interfacing between MovieScene and Sequencer: UMovieSceneSection -> ISequencerSection
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const override;
	virtual void Tick(float DeltaTime) override;
	virtual const FSlateBrush* GetIconBrush() const override;
	// ~End of FMovieSceneTrackEditor interfaces

private:

	/** Callback for AnimatablePropertyChanged in HandleAssetAdded for attached media sources. */
	FKeyPropertyResult AddAttachedECVAsset(FFrameNumber KeyTime, class UEvercoastECVAsset* Asset, TArray<TWeakObjectPtr<UObject>> ObjectsToAttachTo, int32 RowIndex);

	/** Callback for AnimatablePropertyChanged in HandleAssetAdded for master media sources. */
	FKeyPropertyResult AddMasterECVAsset(FFrameNumber KeyTime, class UEvercoastECVAsset* Asset, int32 RowIndex);

	void AddNewSection(const FAssetData& Asset, UECVAssetTrack* Track);
	void AddNewSectionEnterPressed(const TArray<FAssetData>& Asset, UECVAssetTrack* Track);

	void HandleAddECVTrackMenuEntryExecute();

private:
	TSharedPtr<FTrackEditorThumbnailPool> ThumbnailPool;
};