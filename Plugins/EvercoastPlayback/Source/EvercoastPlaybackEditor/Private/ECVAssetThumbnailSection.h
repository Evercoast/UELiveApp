#pragma once

#include "Sections/ThumbnailSection.h"
#include "Templates/SharedPointer.h"
#include "TrackEditorThumbnail/TrackEditorThumbnail.h"
#include "UObject/GCObject.h"
#include "UObject/SoftObjectPtr.h"

class UEvercoastStreamingReaderComp;
class UECVAssetTrackSection;
/**
 * Implements a thumbnail section for Evercoast ECV tracks.
 */
class FECVAssetThumbnailSection
	: public FGCObject
	, public FThumbnailSection
	, public ICustomThumbnailClient
{
public:
	FECVAssetThumbnailSection(UECVAssetTrackSection& InSection, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, TSharedPtr<ISequencer> InSequencer);

public:
	//~ FGCObject interface

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FECVAssetThumbnailSection");
	}
public:
	//~ FThumbnailSection interface
	virtual FMargin GetContentPadding() const override;
	virtual float GetSectionHeight() const override;
	virtual FText GetSectionTitle() const override;
	virtual void SetSingleTime(double GlobalTime) override;
	virtual int32 OnPaintSection(FSequencerSectionPainter& InPainter) const override;

private:

	UEvercoastStreamingReaderComp* GetTemplateECVReader() const;
	void DrawFilmBorder(FSequencerSectionPainter& InPainter, FVector2D SectionSize) const;
	void DrawLoopIndicators(FSequencerSectionPainter& InPainter, FTimespan ECVAssetDuration, FVector2D SectionSize) const;

	/** The section object that owns this section. */
	TWeakObjectPtr<UECVAssetTrackSection> SectionPtr;

	/** The sequencer object that owns this section. */
	TWeakPtr<ISequencer> SequencerPtr;
};
