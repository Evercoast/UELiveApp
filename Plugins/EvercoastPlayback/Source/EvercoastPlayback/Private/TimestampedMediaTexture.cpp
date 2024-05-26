#include "TimestampedMediaTexture.h"
#include "IMediaClock.h"
#include "IMediaClockSink.h"
#include "IMediaModule.h"
#include "MediaPlayerFacade.h"
#include "IMediaPlayer.h"
#include "MediaPlayer.h"
#include "CortoMeshRendererComp.h" // logger
#include "EvercoastPlaybackUtils.h"

class FMyMediaTextureClockSink
	: public IMediaClockSink
{
public:

	FMyMediaTextureClockSink(UTimestampedMediaTexture& InOwner)
		: Owner(&InOwner)
	{ }

	virtual ~FMyMediaTextureClockSink() { }

public:

	virtual void TickRender(FTimespan DeltaTime, FTimespan Timecode) override
	{
		if (UTimestampedMediaTexture* OwnerPtr = Owner.Get())
		{
			OwnerPtr->TickMyResource(Timecode);
		}
	}

private:

	TWeakObjectPtr<UTimestampedMediaTexture> Owner;
};

UTimestampedMediaTexture::UTimestampedMediaTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	DefaultGuid(FGuid::NewGuid())
{
#if PLATFORM_ANDROID
	// On Android, set NewStyleOutput = true will force NOT using external textures, which causes blank texel issue
	NewStyleOutput = true;
#endif
}

FTextureResource* UTimestampedMediaTexture::CreateResource()
{
	// Hook up clock sink so that we can use our own TickMyResource
	if (!MyClockSink.IsValid())
	{
		IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

		if (MediaModule != nullptr)
		{
			MyClockSink = MakeShared<FMyMediaTextureClockSink, ESPMode::ThreadSafe>(*this);
			MediaModule->GetClock().AddSink(MyClockSink.ToSharedRef());
		}
	}

	return Super::CreateResource();
}

void UTimestampedMediaTexture::TickMyResource(FTimespan Timecode)
{
	if (GetResource() == nullptr)
	{
		return;
	}

	// receiver bookkeeping
	UpdateSampleReceiver();

	if (UMediaPlayer* CurrentPlayerPtr = CurrentPlayer.Get())
	{
		const bool PlayerActive = CurrentPlayerPtr->IsPaused() || CurrentPlayerPtr->IsPlaying() || CurrentPlayerPtr->IsPreparing();

		if (PlayerActive)
		{
			check(CurrentPlayerPtr->GetPlayerFacade()->GetPlayer());
			if (CurrentPlayerPtr->GetPlayerFacade()->GetPlayer()->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::UsePlaybackTimingV2))
			{
				TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> Sample;
				while (m_sampleReceiver->Dequeue(Sample))
				{
					if (!Sample.IsValid())
						break;

					// UE_LOG(EvercoastRendererLog, Log, TEXT("Sample timestamp: %.3f"), GetTime().Time.GetTotalSeconds());
				}

				if (!Sample.IsValid())
				{
					return;
				}

				SaveSampleInfo(Sample);
			}
			else
			{
				// legacy mode, haven't tested!
				// Old style: pass queue along and dequeue only at render time
				TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> Sample;
				if (m_sampleReceiver->Peek(Sample))
				{
					if (!Sample.IsValid())
					{
						return;
					}
					SaveSampleInfo(Sample);
				}
			}
		}
	}
	
}

void UTimestampedMediaTexture::SetMediaPlayer(UMediaPlayer* NewMediaPlayer)
{
	Super::SetMediaPlayer(NewMediaPlayer);
	CurrentPlayer = NewMediaPlayer;
	UpdateSampleReceiver();
}

void UTimestampedMediaTexture::UpdateSampleReceiver()
{
	if (UMediaPlayer* CurrentPlayerPtr = CurrentPlayer.Get())
	{
		const FGuid PlayerGuid = CurrentPlayerPtr->GetGuid();

		// Player changed?
		if (CurrentGuid != PlayerGuid)
		{
			m_sampleReceiver = MakeShared<FMediaTextureSampleQueue, ESPMode::ThreadSafe>();
			CurrentPlayerPtr->GetPlayerFacade()->AddVideoSampleSink(m_sampleReceiver.ToSharedRef());

			CurrentGuid = PlayerGuid;
		}
	}
	else
	{
		// No player. Did we already reset to default?
		if (CurrentGuid != DefaultGuid)
		{
			// No, do so now...
			m_sampleReceiver.Reset();
			CurrentGuid = DefaultGuid;
		}
	}
}


FMediaTimeStamp UTimestampedMediaTexture::GetTime() const
{
	if (m_mediaTextureSample)
	{
		return m_mediaTextureSample->GetTime();
	}

	static FMediaTimeStamp Invalid;
	return Invalid;
}

int64_t UTimestampedMediaTexture::GetFrameIndex(double frameRate) const
{
	if (m_mediaTextureSample)
	{
		return ::GetFrameIndex(m_mediaTextureSample->GetTime().Time.GetTotalSeconds(), frameRate);
	}

	return -1;
}


void UTimestampedMediaTexture::SaveSampleInfo(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample)
{
	m_mediaTextureSample = Sample;
	OnSampleUpdated.Broadcast(m_mediaTextureSample->GetTime().Time.GetTotalSeconds());
}