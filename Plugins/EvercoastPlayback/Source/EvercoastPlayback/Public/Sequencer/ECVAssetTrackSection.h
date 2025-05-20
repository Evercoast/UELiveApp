#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "MovieSceneSection.h"
#include "MovieScene.h"
#include "ECVAssetTrackSection.generated.h"

class AEvercoastVolcapActor;
class UEvercoastECVAsset;
class UEvercoastStreamingReaderComp;
class UCortoMeshRendererComp;
class UEvercoastVoxelRendererComp;
class UEvercoastRendererSelectorComp;
class AActor;

UCLASS()
class EVERCOASTPLAYBACK_API UECVAssetTrackSection
	: public UMovieSceneSection
{
	GENERATED_BODY()
public:
	UECVAssetTrackSection(const FObjectInitializer& InInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evercoast Media")
	UEvercoastECVAsset* Asset;

	/*
	// Use TSoftObjectPtr to allow cross level reference
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evercoast Media")
	TSoftObjectPtr<AEvercoastVolcapActor> ReaderRendererActor;
	*/

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evercoast Control")
	FFrameNumber StartFrameOffset;

public:
	// ~Start of UMovieSceneSection public interfaces
	virtual void PostInitProperties() override;
	// ~End of UMovieSceneSection public interfaces

protected:
	// TODO: split and trim
	//~ Start of UMovieSceneSection interface
	virtual void TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft, bool bDeleteKeys) override;
	virtual UMovieSceneSection* SplitSection(FQualifiedFrameTime SplitTime, bool bDeleteKeys) override;
	virtual TOptional<FFrameTime> GetOffsetTime() const override;
	//~ End of UMovieSceneSection interface

protected:
	//~ Begin of UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End of UObject interface

};
