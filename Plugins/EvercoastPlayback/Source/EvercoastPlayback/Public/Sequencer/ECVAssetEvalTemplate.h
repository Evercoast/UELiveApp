#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "ECVAssetTrack.h"
#include "ECVAssetTrackSection.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "ECVAssetEvalTemplate.generated.h"

class AEvercoastVolcapActor;
class UEvercoastStreamingReaderComp;
class UEvercoastECVAsset;
class UEvercoastVoxelRendererComp;
class UCortoMeshRendererComp;
class UEvercoastRendererSelectorComp;
class AActor;

USTRUCT()
struct EVERCOASTPLAYBACK_API FECVAssetTrackSectionParams
{
	GENERATED_BODY()

	UPROPERTY()
	UEvercoastECVAsset* Asset;

	UPROPERTY()
	FFrameNumber SectionStartFrame;

	UPROPERTY()
	FFrameNumber SectionEndFrame;

	UPROPERTY()
	FFrameNumber StartFrameOffset;

	UPROPERTY(Transient)
	UEvercoastECVAsset* SavedAsset;

	FGuid ObjectBindingID;

	FMovieSceneSequenceID SequenceID;

	FTimespan FullDuration;

	bool bPerformedInitialSeek;

	FECVAssetTrackSectionParams()
		:
		Asset(nullptr)
		, SavedAsset(nullptr)
		, FullDuration(0)
		, bPerformedInitialSeek(false)
	{}
};


// There shouldn't be a lot to do here, since evaluation and caching are mainly done in the reader components
USTRUCT()
struct EVERCOASTPLAYBACK_API FECVAssetEvalTemplate
	: public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FECVAssetEvalTemplate() {}

	FECVAssetEvalTemplate(const UECVAssetTrackSection& InSection, const UECVAssetTrack& InTrack);
public:
	// ~Start of FMovieSceneEvalTemplate interfaces
	virtual void Initialize(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
	// Generate tokens that can actually seek the ECV reader
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	virtual void Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
	virtual void SetupOverrides() override;
	virtual void TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
	UScriptStruct& GetScriptStructImpl() const override;

	// ~End of FMovieSceneEvalTemplate interfaces

private:
	UPROPERTY()
	mutable FECVAssetTrackSectionParams Params; // because Setup() is const..

	
};

