#include "Sequencer/ECVAssetEvalTemplate.h"
#include "Sequencer/ECVAssetPersistentData.h"
#include "EvercoastStreamingReaderComp.h"
#include "EvercoastVoxelRendererComp.h"
#include "CortoMeshRendererComp.h"
#include "EvercoastRendererSelectorComp.h"
#include "EvercoastVolcapActor.h"

void FECVAssetTrackSectionParams::SaveActorProperties()
{
	if (ReaderRendererActor && !ReaderRendererActor->IsActorBeingDestroyed())
	{
		SavedReaderRendererActor = ReaderRendererActor.LoadSynchronous();
		if (SavedReaderRendererActor && !SavedReaderRendererActor->IsActorBeingDestroyed())
		{
			SavedAsset = SavedReaderRendererActor->Reader->ECVAsset;
		}
	}
}

void FECVAssetTrackSectionParams::RestoreActorProperties() const
{
	if (ReaderRendererActor && !ReaderRendererActor->IsActorBeingDestroyed())
	{
		if (SavedReaderRendererActor && SavedReaderRendererActor.LoadSynchronous())
		{
			SavedReaderRendererActor->Reader->ECVAsset = SavedAsset;
		}
	}
}


struct FECVAssetPreRollExecutionToken
	: IMovieSceneExecutionToken
{
public:
	FECVAssetPreRollExecutionToken(UEvercoastECVAsset* InAsset, FTimespan InStartTime, FTimespan InFullDuration) :
		TheAsset(InAsset), StartTime(InStartTime),
		FullDuration(InFullDuration)
	{
		
	}
	// Prepare the playback
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		FECVAssetPersistentData& SectionData = PersistentData.GetSectionData<FECVAssetPersistentData>();

		UEvercoastStreamingReaderComp* Reader = SectionData.GetEvercoastReader();
		if (!Reader || !TheAsset)
		{
			return;
		}

		check(!Reader->IsBeingDestroyed());

		// get duration
		if (Reader->ECVAsset != TheAsset)
		{

			// crop StartTime
			FTimespan LocalStartTime = StartTime;
			while (LocalStartTime  > FullDuration && !FullDuration.IsZero())
			{
				LocalStartTime -= FullDuration;
			}

			Reader->ECVAsset = TheAsset;
			Reader->SetPlaybackStartTime(LocalStartTime.GetTotalSeconds());
			Reader->RecreateReaderSync();
			Reader->StreamingPlay();
			Reader->StreamingPause();
		}
	}

private:
	UEvercoastECVAsset* TheAsset;
	FTimespan StartTime;
	FTimespan FullDuration;
};


struct FECVAssetExecutionToken
	: IMovieSceneExecutionToken
{
	FECVAssetExecutionToken(UEvercoastECVAsset* InAsset, FTimespan InCurrTime, FTimespan InFrameDuration, FTimespan InFullDuration) :
		TheAsset(InAsset), CurrTime(InCurrTime), FrameDuration(InFrameDuration), 
		FullDuration(InFullDuration)
	{
	}

public:
	// Run/Seek the playback to position
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		FECVAssetPersistentData& SectionData = PersistentData.GetSectionData<FECVAssetPersistentData>();

		UEvercoastStreamingReaderComp* Reader = SectionData.GetEvercoastReader();
		if (!Reader || !TheAsset)
			return;

		check(!Reader->IsBeingDestroyed());

		// CurrTime is relative timestamp based on the start of the section, when performing SeekTo() and SetPlaybackStartTime() 
		// we need to crop it back within the range of clip duration in order to do SeekTo() or SetPlaybackStartTime()
		// SetPlaybackMicroTimeManagement() still uses CurrTime
		FTimespan LocalCurrTime = CurrTime;
		while (LocalCurrTime > FullDuration && !FullDuration.IsZero())
		{
			LocalCurrTime -= FullDuration;
		}


		// get duration
		if (Reader->ECVAsset != TheAsset)
		{
			Reader->ECVAsset = TheAsset;
			Reader->SetPlaybackStartTime(LocalCurrTime.GetTotalSeconds());
			Reader->SetPlaybackMicroTimeManagement(CurrTime.GetTotalSeconds(), FrameDuration.GetTotalSeconds(), (CurrTime + FrameDuration).GetTotalSeconds());
			Reader->RecreateReaderSync();
			Reader->StreamingPlay();
			Reader->StreamingPause();
			return;
		}

		Reader->SetPlaybackStartTime(LocalCurrTime.GetTotalSeconds());
		Reader->SetPlaybackMicroTimeManagement(CurrTime.GetTotalSeconds(), FrameDuration.GetTotalSeconds(), (CurrTime + FrameDuration).GetTotalSeconds());
		if (Context.GetStatus() == EMovieScenePlayerStatus::Playing)
		{
			// play
			//UE_LOG(EvercoastReaderLog, Log, TEXT("Seq curr time: %.3f, one frame time: %.3f"), CurrTime.GetTotalSeconds(), (CurrTime + FrameDuration).GetTotalSeconds());

			if (!Reader->IsStreamingPlaying())
			{
				// haven't been playing
				Reader->StreamingPlay();
				Reader->StreamingSeekTo(LocalCurrTime.GetTotalSeconds());
			}
			else
			{
				// already playing, taking care of jumping/reverse play
				if (Context.HasJumped())
				{
					// only seek when context jumped
					Reader->StreamingSeekTo(LocalCurrTime.GetTotalSeconds());
				}

				// Ghost tree reader doesn't support backwards streaming/reading
				if (Context.GetDirection() == EPlayDirection::Forwards)
				{
					if (!Reader->IsStreamingPlaying())
						Reader->StreamingPlay();
				}
				else
				if (Context.GetDirection() == EPlayDirection::Backwards)
				{
					Reader->StreamingPause();
					Reader->StreamingSeekTo(LocalCurrTime.GetTotalSeconds());
				}
				
			}
		}
		else
		{
			// pause and seek
			if (Reader->IsStreamingPlaying())
			{
				Reader->StreamingPause();
			}

			Reader->StreamingSeekTo(LocalCurrTime.GetTotalSeconds());

			Reader->RecalcequencerOverrideLoopCount();
		}
	}

private:
	UEvercoastECVAsset* TheAsset;
	FTimespan CurrTime;
	FTimespan FrameDuration;
	FTimespan FullDuration;
};

FECVAssetEvalTemplate::FECVAssetEvalTemplate(const UECVAssetTrackSection& InSection, const UECVAssetTrack& InTrack)
{
	Params.Asset = InSection.Asset;

	// keep track of Reader and Renderer
	Params.ReaderRendererActor = InSection.ReaderRendererActor;
	Params.ReaderRendererActor.LoadSynchronous();
	Params.StartFrameOffset = InSection.StartFrameOffset;

	if (InSection.HasStartFrame())
	{
		Params.SectionStartFrame = InSection.GetRange().GetLowerBoundValue();
	}
	if (InSection.HasEndFrame())
	{
		Params.SectionEndFrame = InSection.GetRange().GetUpperBoundValue();
	}
}


void FECVAssetEvalTemplate::Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	if (Params.ReaderRendererActor)
	{
		Params.ReaderRendererActor.LoadSynchronous();
	}

	auto Reader = Params.ReaderRendererActor ? Params.ReaderRendererActor->Reader : nullptr;

	PersistentData.AddSectionData<FECVAssetPersistentData>().AcquireActor(Params.ReaderRendererActor);

	// After fetching the reader, save auto created renderer when this starts, later we have to restore it
	Params.SaveActorProperties();

	if (Reader)
	{
		// Get duration
		if (Params.FullDuration.IsZero())
		{
			Reader->ECVAsset = Params.Asset;
			Reader->SetPlaybackStartTime(0);
			Reader->RecreateReaderSync();
			Reader->StreamingPlay();
			Reader->StreamingPause();

			// need to wait for streaming duration became reliable to set the proper Params.FullDuration
			while (!Reader->IsStreamingDurationReliable())
			{
				Reader->WaitForDurationBecomesReliable();
			}

			Params.FullDuration = FTimespan::FromSeconds(Reader->StreamingGetDuration());
		}
	}

}

void FECVAssetEvalTemplate::SetupOverrides()
{
	EnableOverrides(RequiresInitializeFlag | RequiresSetupFlag | RequiresTearDownFlag);
}

void FECVAssetEvalTemplate::Initialize(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	FECVAssetPersistentData* SectionData = PersistentData.FindSectionData<FECVAssetPersistentData>();

	if (!ensure(SectionData != nullptr))
	{
		return;
	}

	// Check for reader and renderer's validity as they can become trashed from editing

	UEvercoastStreamingReaderComp* Reader = nullptr;
	if (Params.ReaderRendererActor && Params.ReaderRendererActor.LoadSynchronous() && !Params.ReaderRendererActor->IsActorBeingDestroyed())
	{
		Reader = Params.ReaderRendererActor->Reader;
	}

	if (!Reader)
		return;

	UEvercoastRendererSelectorComp* Renderer = nullptr;
	if (Params.ReaderRendererActor && Params.ReaderRendererActor.LoadSynchronous() && !Params.ReaderRendererActor->IsActorBeingDestroyed())
	{
		Renderer = Params.ReaderRendererActor->RendererSelector;
	}

	
	UEvercoastStreamingReaderComp* theReader = Reader;

	const bool IsEvaluating = !(Context.IsPreRoll() || Context.IsPostRoll() || (Context.GetTime().FrameNumber >= Params.SectionEndFrame));

	if (theReader)
	{
		if (IsEvaluating)
		{
			// setup asset first
			if (theReader->ECVAsset != Params.Asset)
			{
				theReader->ECVAsset = Params.Asset;
				theReader->RecreateReaderSync();
			}
		}
		else
		{
			// no evaluating, no asset, but will need to set proper renderer
			theReader->ECVAsset = nullptr;
			theReader->RecreateReaderSync();
		}
	}
}

void FECVAssetEvalTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	// Generate corresponding tokens
	FECVAssetPersistentData* SectionData = PersistentData.FindSectionData<FECVAssetPersistentData>();
	if (!ensure(SectionData != nullptr))
	{
		return;
	}

	if (Context.IsPreRoll())
	{
		const FFrameRate FrameRate = Context.GetFrameRate();
		const FFrameNumber StartFrame = Context.HasPreRollEndTime() ? Context.GetPreRollEndFrame() - Params.SectionStartFrame + Params.StartFrameOffset : Params.StartFrameOffset;
		const int64 DenominatorTicks = FrameRate.Denominator * ETimespan::TicksPerSecond;
		const int64 StartTicks = FMath::DivideAndRoundNearest(int64(StartFrame.Value * DenominatorTicks), int64(FrameRate.Numerator));

		ExecutionTokens.Add(FECVAssetPreRollExecutionToken(Params.Asset, FTimespan(StartTicks), Params.FullDuration));

	}
	else if (!Context.IsPostRoll() && (Context.GetTime().FrameNumber < Params.SectionEndFrame))
	{
		const FFrameRate FrameRate = Context.GetFrameRate();
		const FFrameTime FrameTime(Context.GetTime().FrameNumber - Params.SectionStartFrame + Params.StartFrameOffset);
		const int64 DenominatorTicks = FrameRate.Denominator * ETimespan::TicksPerSecond;
		const int64 FrameTicks = FMath::DivideAndRoundNearest(int64(FrameTime.GetFrame().Value * DenominatorTicks), int64(FrameRate.Numerator));
		const int64 FrameSubTicks = FMath::DivideAndRoundNearest(int64(FrameTime.GetSubFrame() * DenominatorTicks), int64(FrameRate.Numerator));
		const int64 FrameDurationTicks = 1000 * FMath::DivideAndRoundNearest(DenominatorTicks, int64(FrameRate.Numerator));

		// NOTE: this FrameDurationTicks is the duration in ticks of ONE frame, it's not meant to be used to limit the playback of the section
		// It's used for frame-by-frame limit, which isn't plausible for video playback
		ExecutionTokens.Add(FECVAssetExecutionToken(Params.Asset, FTimespan(FrameTicks + FrameSubTicks), FTimespan(FrameDurationTicks), Params.FullDuration));
	}
	else
	{
		// do nothing
	}
}


void FECVAssetEvalTemplate::TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	FECVAssetPersistentData* SectionData = PersistentData.FindSectionData<FECVAssetPersistentData>();

	if (!ensure(SectionData != nullptr))
	{
		return;
	}

	// restore the old reader
	Params.RestoreActorProperties();

	// teardown when section playback is finished
	if (Params.SavedReaderRendererActor)
	{
		Params.SavedReaderRendererActor.LoadSynchronous();
	}

	UEvercoastStreamingReaderComp* theReader = Params.SavedReaderRendererActor ? Params.SavedReaderRendererActor->Reader : nullptr;
	if (theReader && !theReader->IsBeingDestroyed())
	{
		theReader->RecreateReaderSync();
		theReader->RemovePlaybackMicroTimeManagement();
	}

	SectionData->GiveupActor();
}

UScriptStruct& FECVAssetEvalTemplate::GetScriptStructImpl() const
{
	return *StaticStruct();
}