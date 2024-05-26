#include "ElectraVideoTextureHog.h"

#include "GhostTreeFormatReader.h"
#include "EvercoastPlaybackUtils.h"
#include "TimestampedMediaTexture.h"
#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"
#include "IMediaPlayer.h"
#include "StreamMediaSource.h"
#include "FileMediaSource.h"
#include "HttpManager.h"

UElectraVideoTextureHog::UElectraVideoTextureHog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UObject* transientPackage = (UObject*)GetTransientPackage();
	FName mediaPlayerName = MakeUniqueObjectName(transientPackage, UMediaPlayer::StaticClass(), FName(this->GetName() + TEXT("_MediaPlayer")));

	m_mediaPlayer = NewObject<UMediaPlayer>(transientPackage, mediaPlayerName);
	m_mediaPlayer->PlayOnOpen = false;
	m_mediaPlayer->CacheAhead = FTimespan::FromMilliseconds(4000);
	m_mediaPlayer->CacheBehind = FTimespan::FromMilliseconds(4000);
	m_mediaPlayer->CacheBehindGame = FTimespan::FromMilliseconds(4000);
	m_mediaPlayer->GetPlayerFacade()->DesiredPlayerName = FName(TEXT("ElectraPlayer"));
	
	FName mediaTextureName = MakeUniqueObjectName(transientPackage, UTimestampedMediaTexture::StaticClass(), FName(this->GetName() + TEXT("_TimestamppedMediaTexture")));
	m_mediaTexture = NewObject<UTimestampedMediaTexture>(transientPackage, mediaTextureName);
	m_mediaTexture->UpdateResource();
	// init m_textureBuffer later when we know the media's sample rate

	m_textureBufferStart = 0;
	m_textureBufferEnd = 0;
	m_mediaFrameRate = 0;
	m_lastFrameIndex = -1;
	m_mediaOpened = false;
	m_mediaEndReached = false;
	m_mediaSeekStatus = MediaSeekStatus::Seek_Uninitialised;
	m_mediaPendingSeek = 0;
	m_lastMediaTextureTimestamp = 0;

	m_hoggingStoppedDueToFullBuffer = false;

#if PLATFORM_ANDROID
	m_supposedMediaPlayerStatus = MPS_STOPPED;
	m_lastMediaPlayerStatusCheckTime = 0;
#endif
}

UElectraVideoTextureHog::~UElectraVideoTextureHog()
{
    Destroy();
}

void UElectraVideoTextureHog::Destroy()
{
	for (int i = 0; i < m_textureBuffer.Num(); ++i)
	{
		m_textureBuffer[i]->FreeTexture();
		m_textureBuffer[i]->RemoveFromRoot();
	}

	m_textureBuffer.Empty();
}

void UElectraVideoTextureHog::PrepareForOpening()
{
	m_mediaOpenPromise = std::promise<void>();
	m_mediaOpenFuture = m_mediaOpenPromise.get_future();
	m_mediaOpened = false;
	m_mediaEndReached = false;

	m_mediaTexture->SetMediaPlayer(m_mediaPlayer);
#if WITH_EDITOR
	m_mediaTexture->SetDefaultMediaPlayer(m_mediaPlayer);
#endif
	m_mediaTexture->UpdateResource();
	m_mediaTexture->OnSampleUpdated.AddUniqueDynamic(this, &UElectraVideoTextureHog::OnMediaTextureSampleUpdated);

	m_mediaPlayer->OnMediaOpened.AddUniqueDynamic(this, &UElectraVideoTextureHog::OnMediaOpened);
	m_mediaPlayer->OnMediaOpenFailed.AddUniqueDynamic(this, &UElectraVideoTextureHog::OnMediaOpenFailed);
	m_mediaPlayer->OnEndReached.AddUniqueDynamic(this, &UElectraVideoTextureHog::OnMediaEndReached);
	m_mediaPlayer->OnSeekCompleted.AddUniqueDynamic(this, &UElectraVideoTextureHog::OnMediaSeekCompleted);
}

bool UElectraVideoTextureHog::OpenFile(const FString& filePath)
{
	PrepareForOpening();

	UFileMediaSource* mediaSource = NewObject<UFileMediaSource>();
	mediaSource->FilePath = filePath;

	m_lastMediaSource = mediaSource;
	return m_mediaPlayer->OpenSource(mediaSource);
}


bool UElectraVideoTextureHog::OpenUrl(const FString& url)
{
	PrepareForOpening();

	check(m_mediaPlayer->CanPlayUrl(url));

	UStreamMediaSource* mediaSource = NewObject<UStreamMediaSource>();
	mediaSource->StreamUrl = url;
	// FIXME: rebuffering happens for streaming URL when buffer underrun(network too slow) but adapative streaming reader just ignored the saved timestamp 
	// and cannot start the rebuffering at the correct timestamp
	//mediaSource->SetMediaOptionBool(FName("ElectraThrowErrorWhenRebuffering"), true);

	m_lastMediaSource = mediaSource;

	return m_mediaPlayer->OpenSource(mediaSource);
}

bool UElectraVideoTextureHog::OpenSource(UMediaSource* source)
{
	PrepareForOpening();

	check(m_mediaPlayer->CanPlaySource(source));

	m_lastMediaSource = source;
	return m_mediaPlayer->OpenSource(source);
}

bool UElectraVideoTextureHog::IsVideoOpened()
{
	if (m_mediaOpenFuture.valid())
	{
		return m_mediaOpenFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
	}
	return false;
}

void UElectraVideoTextureHog::OnMediaOpened(FString openedUrl)
{
	UE_LOG(EvercoastReaderLog, Log, TEXT("Opened: %s"), *openedUrl);

	auto facade = m_mediaPlayer->GetPlayerFacade();

	// check if ElectraPlayer was selected
	FName playerName = facade->GetPlayerName();
	if (!playerName.IsEqual(FName("ElectraPlayer")))
	{
		UE_LOG(EvercoastReaderLog, Warning, TEXT("%s was used instead of ElectraPlayer. Video decoding might not be working properly."), *playerName.ToString());
	}


	if (!facade->GetPlayer()->GetPlayerFeatureFlag(IMediaPlayer::EFeatureFlag::UsePlaybackTimingV2))
	{
		UE_LOG(EvercoastReaderLog, Warning, TEXT("Use legacy playback timing V1"));
	}
	else
	{
		UE_LOG(EvercoastReaderLog, Log, TEXT("Use playback timing V2"));
	}

	m_mediaOpened = true;
	m_mediaEndReached = false;
	m_mediaFrameRate = m_mediaPlayer->GetVideoTrackFrameRate(INDEX_NONE, INDEX_NONE);
	FIntPoint dim = m_mediaPlayer->GetVideoTrackDimensions(INDEX_NONE, INDEX_NONE);

	// we need a ring queue that can fit at least 2 second of data
	// (Defaultly we encoded with one keyframe per 30 frames, so 1 keyframe per 2 seconds)
	if (m_mediaFrameRate * 2 > m_textureBuffer.Num())
	{ 
		// Remove ref before we change the container
		for (int i = 0; i < m_textureBuffer.Num(); ++i)
		{
			m_textureBuffer[i]->RemoveFromRoot();
		}

		RING_QUEUE_SIZE = (int)(m_mediaFrameRate * 2 + 0.5f);

		m_textureBuffer.SetNumZeroed(RING_QUEUE_SIZE, true);
		for (int i = 0; i < m_textureBuffer.Num(); ++i)
		{
			m_textureBuffer[i] = NewObject<UTextureRecord>(GetTransientPackage());
			// Keep object ref
			m_textureBuffer[i]->AddToRoot();
		}
		
	}
	for (int i = 0; i < m_textureBuffer.Num(); ++i)
	{
		m_textureBuffer[i]->FreeTexture();
		m_textureBuffer[i]->InitTexture(dim.X, dim.Y, i);
	}

	UE_LOG(EvercoastReaderLog, Log, TEXT("MediaPlayer supports seeking: %d"), m_mediaPlayer->SupportsSeeking());
	UE_LOG(EvercoastReaderLog, Log, TEXT("MediaPlayer supports scrubbing: %d"), m_mediaPlayer->SupportsScrubbing());

	ResetTo(m_mediaPendingSeek);
	// Start hogging regardless, we need a first frame
	StartHogging();

	m_mediaOpenPromise.set_value();
}

void UElectraVideoTextureHog::OnMediaOpenFailed(FString failedUrl)
{
	UE_LOG(EvercoastReaderLog, Warning, TEXT("Open failed: %s"), *failedUrl);

	m_mediaOpened = false;
	m_mediaOpenPromise.set_value();
}

void UElectraVideoTextureHog::OnMediaEndReached()
{
	UE_LOG(EvercoastReaderLog, Warning, TEXT("End reached"));

	// we cannot just reset to time 0, this is cached 'end event' so the end will come a few sec later
	m_mediaEndReached = true;
	
}

void UElectraVideoTextureHog::OnMediaTextureSampleUpdated(double timestamp)
{
	// Cannot grab the frame here as the sample just arrived and haven't been decoded to texture
	//HogCurrentFrameAndDoBookkeeping();
	
	
	// NOTE: do not set seek_completed here.
	// This media texture's timestamp could still be the one staying in the output queue, after seek+flush calls, so it could be just
	// the 'next' timestamp. If the seek is very close to the current timestamp, and because of big gap the player will eventually goto
	// the next keyframe position(could be well far apart from the requested pos), leaving here to incorrectly set to seek_completed.
	if (abs(timestamp - m_lastMediaTextureTimestamp) >= 1.0)
	{
		UE_LOG(EvercoastReaderLog, Warning, TEXT("Big gap detected, this could caused by loop, or by both accurate/inaccurate seek: %.3f - %.3f - %.3f"), timestamp, m_mediaPendingSeek, m_lastMediaTextureTimestamp);
		// still big gap, keep fast forwarding. do not worry about the normal seeking and loop, this status will be cleared in Tick()
		m_mediaSeekStatus = MediaSeekStatus::Seek_TexUpdated_BigGap;
	}

	m_lastMediaTextureTimestamp = timestamp;
}

void UElectraVideoTextureHog::OnMediaSeekCompleted()
{
	// NOTE: can't trust current timestamp here as everything is async and no param is reported here
	// so instead just mark seek as completed but inaccurate and recheck it during tick
	m_mediaSeekStatus = MediaSeekStatus::Seek_Completed_BigGap;

}

bool UElectraVideoTextureHog::Close()
{
	ResetTo(0);
	m_mediaPlayer->Close();
#if PLATFORM_ANDROID
	m_supposedMediaPlayerStatus = MPS_STOPPED;
#endif
	// Probably don't want to free texture as the editor mesh component still needs it after play has stopped,
	// Or the game runtime mesh component just need it for keeping its last rendered frame
	for (int i = 0; i < m_textureBuffer.Num(); ++i)
	{
		m_textureBuffer[i]->SetFrameTimestamp(-1, -1);
		m_textureBuffer[i]->MarkAsUsed(false);
	}
	return true;
}

bool UElectraVideoTextureHog::ResetTo(double timestamp, const std::function<void()>& callback)
{
	if (timestamp < 0)
		timestamp = 0;


	// Cache the seek request if timestamp match
	// Need to be careful so that it won't ignore valid seek request
	if (m_mediaSeekStatus == MediaSeekStatus::Seek_Completed && m_mediaPendingSeek == timestamp)
	{
		if (!m_textureBuffer[m_textureBufferStart]->isUsed &&
			abs(m_textureBuffer[m_textureBufferStart]->frameTimestamp - timestamp) <= 1.0 / m_mediaFrameRate)
		{
			return true;
		}
	}

	m_mediaPendingSeek = timestamp;

	if (!m_mediaOpened)
	{
		return false;
	}

	m_mediaEndReached = false;
	m_mediaSeekStatus = MediaSeekStatus::Seek_Requested;
	m_mediaPlayer->Seek(FTimespan::FromSeconds(timestamp));

	// clear the buffer
	for (int i = 0; i < m_textureBuffer.Num(); ++i)
	{
		m_textureBuffer[i]->SetFrameTimestamp(-1, -1);
		m_textureBuffer[i]->MarkAsUsed(false);
	}
	// NOTE: it seems if we reset this checker variable, it will likely grab the last frame before we issue seeks, which will results in incorrect buffer contents which can affects the video frame matching algorithm
	//m_lastFrameIndex = -1; 
	m_textureBufferStart = 0;
	m_textureBufferEnd = 0;
	m_hoggingStoppedDueToFullBuffer = false;

	m_seekingCompletedCallback = callback;

	return true;
}

bool UElectraVideoTextureHog::JumpBy(double timestampOffset, const std::function<void()>& callback)
{
	if (!m_mediaOpened)
	{
		return false;
	}

	return ResetTo(m_mediaPlayer->GetTime().GetTotalSeconds() + timestampOffset, callback);
}

bool UElectraVideoTextureHog::StartHogging()
{
	if (!m_mediaOpened)
	{
		return false;
	}

	// What you cannot trust IsPlaying???
	//if (!m_mediaPlayer->IsPlaying())
	{
		UE_LOG(EvercoastReaderLog, Verbose, TEXT("STARTED HOGGING"));
#if PLATFORM_ANDROID
		m_supposedMediaPlayerStatus = MPS_PLAYING;
#endif
		return m_mediaPlayer->Play();
	}
	return true;
}

void UElectraVideoTextureHog::HogCurrentFrameAndDoBookkeeping()
{
	int64_t frameIndex = GetCurrentFrameIndex();
	if (frameIndex != m_lastFrameIndex)
	{
		if (HogCurrentFrame())
		{
			//UE_LOG(EvercoastReaderLog, Log, TEXT("== Hogged frame at %d - %.4f =="), frameIndex, GetCurrentFrameTimestamp());
			m_lastFrameIndex = frameIndex;
		}
		else
		{
			UE_LOG(EvercoastReaderLog, Verbose, TEXT("Cannot hog more frames. Buffer is full. Contents:"));
			for (int i = 0; i < m_textureBuffer.Num(); ++i)
			{
				UE_LOG(EvercoastReaderLog, Verbose, TEXT("buffer[%d]: %lld - %.3f Used: %d"), i, m_textureBuffer[i]->frameIndex, m_textureBuffer[i]->frameTimestamp, m_textureBuffer[i]->isUsed);
			}
			// FIXME: move this stop hogging call to allow client to decide
			PauseHoggingDueToFull();
		}
	}
}

bool UElectraVideoTextureHog::PauseHoggingDueToFull()
{
	m_hoggingStoppedDueToFullBuffer = true;
	return StopHogging();
}

bool UElectraVideoTextureHog::IsHoggingPausedDueToFull() const
{
	return m_mediaOpened && !m_mediaPlayer->IsPlaying() && m_hoggingStoppedDueToFullBuffer;
}

bool UElectraVideoTextureHog::StopHogging()
{
	if (!m_mediaOpened)
	{
		return false;
	}

	if (m_mediaPlayer->IsPlaying())
	{
		UE_LOG(EvercoastReaderLog, Verbose, TEXT("STOPPED HOGGING. Is Full: %d"), m_hoggingStoppedDueToFullBuffer);
#if PLATFORM_ANDROID
		m_supposedMediaPlayerStatus = MPS_STOPPED;
#endif
		return m_mediaPlayer->Pause();
	}
	return true;
}

bool UElectraVideoTextureHog::IsHogging() const
{
	return m_mediaOpened && m_mediaPlayer->IsPlaying() && (m_mediaSeekStatus == MediaSeekStatus::Seek_Completed);
}

bool UElectraVideoTextureHog::IsTryingFastforwardToAccurateSeek() const
{
	return m_mediaOpened && (m_mediaSeekStatus == MediaSeekStatus::Seek_Completed_BigGap || m_mediaSeekStatus == MediaSeekStatus::Seek_TexUpdated_BigGap);
}

bool UElectraVideoTextureHog::IsEndReached() const
{
	return m_mediaEndReached;
}

void UElectraVideoTextureHog::Tick(UWorld* world)
{
	if (IsHogging())
	{
		HogCurrentFrameAndDoBookkeeping();
	}
	else if (IsTryingFastforwardToAccurateSeek())
	{
		// seek target ahead of current recorded timestamp
		if (m_mediaPendingSeek - GetCurrentFrameTimestamp() >= 1.0) 
		{
			// seek target is beyond current timestamp, keep waiting and make sure the player is playing
			UE_LOG(EvercoastReaderLog, Warning, TEXT("Big gap still appears: %.3f - %.3f"), GetCurrentFrameTimestamp(), m_mediaPendingSeek);
			// set to a reasonable fast forwarding rate, even it's paused
			if (!m_mediaPlayer->IsPlaying() )
			{
#if PLATFORM_ANDROID
				m_supposedMediaPlayerStatus = MPS_PLAYING;
#endif
				m_mediaPlayer->Play();
			}
		}
		// current recorded timestamp is ahead of seek target
		else if (GetCurrentFrameTimestamp() - m_mediaPendingSeek >= 1.0)
		{
			// seek target is before current timestamp, cannot wait here otherwise the playing cursor will move further away from the seek target
			// have to jump further before, towards timestamp 0
			if (m_mediaSeekStatus != Seek_Requested && m_mediaSeekStatus != Seek_Completed_BigGap) // need to take care of seek operation as we don't want to keep requesting seek so it never updates the texture
			{
				UE_LOG(EvercoastReaderLog, Warning, TEXT("Big gap and seek target is before current timestamp, need further seeking: %.3f - %.3f"), GetCurrentFrameTimestamp(), m_mediaPendingSeek);
				ResetTo(GetCurrentFrameTimestamp() - 2.0);
			}

			if (!m_mediaPlayer->IsPlaying())
			{
#if PLATFORM_ANDROID
				m_supposedMediaPlayerStatus = MPS_PLAYING;
#endif
				m_mediaPlayer->Play();
			}
		}
		else
		{
			UE_LOG(EvercoastReaderLog, Log, TEXT("Big gap seems to be gone."));
			// close enough, stop fastforwarding
			m_mediaSeekStatus = MediaSeekStatus::Seek_Completed;
		}
	}

#if PLATFORM_ANDROID
	CheckMediaPlayerStatus(world);
#endif
}


#if PLATFORM_ANDROID
void UElectraVideoTextureHog::CheckMediaPlayerStatus(UWorld* world)
{
	// I can't believe I have to write this on Android. 
	// It seems MediaPlayer can randomly ignore your request and stop at its own will, especially after some seeking.
	// So you can't entirely trust the Play() or Pause() calls on Android. Instead, you check the supposed state
	// so that if the state isn't what you'd expected, request another change.
	// Doing this is like holding a carrot to make the donkey move, but it works!
	float gap = world->GetTimeSeconds() - m_lastMediaPlayerStatusCheckTime;
	if (gap > 0.5f)
	{
		// check
		if (m_supposedMediaPlayerStatus == MPS_PLAYING && !m_mediaPlayer->IsPlaying())
		{
			UE_LOG(EvercoastReaderLog, Warning, TEXT("MediaPlayer still not playing. Enforcing now."));
			m_mediaPlayer->SetRate(1.0f);
		}
		if (m_supposedMediaPlayerStatus == MPS_STOPPED && m_mediaPlayer->IsPlaying())
		{
			UE_LOG(EvercoastReaderLog, Warning, TEXT("MediaPlayer still not stopping. Enforcing now."));
			m_mediaPlayer->SetRate(0.0f);
		}
		m_lastMediaPlayerStatusCheckTime = world->GetTimeSeconds();
	}
}
#endif

double UElectraVideoTextureHog::GetCurrentFrameTimestamp() const
{
	FMediaTimeStamp ts = m_mediaTexture->GetTime();
	return ts.Time.GetTotalSeconds();
}

int64_t UElectraVideoTextureHog::GetCurrentFrameIndex() const
{
	return m_mediaTexture->GetFrameIndex(m_mediaFrameRate);
}

bool UElectraVideoTextureHog::IsFull() const
{
	if ((m_textureBufferEnd + 1) % RING_QUEUE_SIZE != m_textureBufferStart)
	{
		return false;
	}

	return true;
}

bool UElectraVideoTextureHog::IsFrameIndexWithinDuration(int64_t frameIndex) const
{
	if (m_mediaOpened)
	{
		int64_t beyondLastFrameIndex = ::GetFrameIndex(m_mediaPlayer->GetDuration().GetTotalSeconds(), m_mediaFrameRate);
		if (frameIndex >= 0 && frameIndex < beyondLastFrameIndex)
		{
			return true;
		}
	}

	return false;
}

bool UElectraVideoTextureHog::HogCurrentFrame()
{
	// find the next free slot in the ring queue,
	if (!IsFull())
	{
		UTimestampedMediaTexture* pMediaTexture = m_mediaTexture;
		int64_t accurateFrameIndex = pMediaTexture->GetFrameIndex(m_mediaFrameRate);
		if (accurateFrameIndex < 0)
		{
			UE_LOG(EvercoastReaderLog, Warning, TEXT("No sample has recorded in UTimestampedMediaTexture yet"));
			// still returning true, we don't want to stop hogging yet, just retry next time
			return true;
		}

		// Copy m_mediaTexture to a regular UTexture2D
		//UTextureRecord* pRec = m_textureBuffer[m_textureBufferEnd];
		m_promise = MakeShared<TPromise<void>, ESPMode::ThreadSafe>();
		auto future = m_promise->GetFuture();
		UTexture* pTex = m_textureBuffer[m_textureBufferEnd]->texture;
		ENQUEUE_RENDER_COMMAND(UElectraVideoTextureHog_CopyTexture)(
			[pMediaTexture, pTex, promise=m_promise](FRHICommandListImmediate& RHICmdList)
			{
				FTextureRHIRef targetRHI = pTex->GetResource()->TextureRHI;
				FTextureRHIRef srcRHI = pMediaTexture->GetResource()->TextureRHI;

				RHICmdList.Transition(FRHITransitionInfo(srcRHI, ERHIAccess::SRVMask, ERHIAccess::CopySrc));
				RHICmdList.Transition(FRHITransitionInfo(targetRHI, ERHIAccess::SRVMask, ERHIAccess::CopyDest));
				RHICmdList.CopyTexture(srcRHI, targetRHI, FRHICopyTextureInfo());
				RHICmdList.Transition(FRHITransitionInfo(targetRHI, ERHIAccess::CopyDest, ERHIAccess::SRVMask));

				promise->SetValue();
			});

		future.Get();

		m_textureBuffer[m_textureBufferEnd]->SetFrameTimestamp(accurateFrameIndex, pMediaTexture->GetTime().Time.GetTotalSeconds());
		m_textureBuffer[m_textureBufferEnd]->MarkAsUsed(false);

		m_textureBufferEnd = (m_textureBufferEnd + 1) % RING_QUEUE_SIZE;

		// only trust media player when actual texture being copied
		m_seekingCompletedCallback();
		m_seekingCompletedCallback = std::bind([=]() {});

		return true;
	}
	else
	{
		// ring queue is full
		return false;
	}
}

UTexture* UElectraVideoTextureHog::QueryTextureAtIndex(int64_t frameIndex) const
{
	const UTextureRecord *const * ppTR = m_textureBuffer.FindByPredicate([frameIndex](const UTextureRecord* tr)
		{
			return tr->frameIndex == frameIndex && !tr->isUsed;
		});
	return ppTR ? (*ppTR)->texture : nullptr;
}

void UElectraVideoTextureHog::RestartHoggingIfPausedDueToFull()
{
	if (m_hoggingStoppedDueToFullBuffer)
	{
		if (StartHogging())
		{
			m_hoggingStoppedDueToFullBuffer = false;
		}
	}
}

bool UElectraVideoTextureHog::InvalidateTextureAndBefore(UTexture* pTex)
{
	int32 IndexFound = m_textureBuffer.IndexOfByPredicate([pTex](const UTextureRecord* tr)
		{
			return tr->texture == pTex;
		}
	);

	if (IndexFound != INDEX_NONE)
	{
		m_textureBuffer[IndexFound]->MarkAsUsed(true);

		// shall we remove all the texture behind this one, till the start of the ring queue
		for (int i = m_textureBufferStart; i != IndexFound; )
		{
			m_textureBuffer[i]->MarkAsUsed(true);
			i = (i + 1) % RING_QUEUE_SIZE;
		}

		// a new start
		m_textureBufferStart = (IndexFound + 1) % RING_QUEUE_SIZE;

		// If we detect the hogging was stopped because the buffer is full, we should
		// resume hogging here, after freeing some slots
		RestartHoggingIfPausedDueToFull();
		return true;
	}
	return false;
}

bool UElectraVideoTextureHog::InvalidateTextureAndBeforeByFrameIndex(int64_t frameIndex)
{
	bool hasFound = false;
	for (int i = 0; i < m_textureBuffer.Num(); ++i)
	{
		auto& record = m_textureBuffer[i];
		if (record->frameIndex <= frameIndex && !record->isUsed)
		{
			hasFound = true;
			record->MarkAsUsed(true);
			m_textureBufferStart = m_textureBufferStart > ((i + 1) % RING_QUEUE_SIZE) ? m_textureBufferStart : ((i + 1) % RING_QUEUE_SIZE);
		}
	}

	RestartHoggingIfPausedDueToFull();
	return hasFound;
}

void UElectraVideoTextureHog::InvalidateAllTextures()
{
	for (int i = 0; i < m_textureBuffer.Num(); ++i)
	{
		m_textureBuffer[i]->SetFrameTimestamp(-1, -1);
		m_textureBuffer[i]->MarkAsUsed(false);
	}

	m_textureBufferStart = 0;
	m_textureBufferEnd = 0;
	RestartHoggingIfPausedDueToFull();
}

bool UElectraVideoTextureHog::IsFrameWithinCachedRange(int64_t frameIndex) const
{
	int64_t rangeStart = std::numeric_limits<int64>::max();
	int64_t rangeEnd = -1;
	for (int i = m_textureBufferStart; i != m_textureBufferEnd; )
	{
		if (!m_textureBuffer[i]->isUsed)
		{
			if (rangeStart > m_textureBuffer[i]->frameIndex)
			{
				rangeStart = m_textureBuffer[i]->frameIndex;
			}

			if (rangeEnd < m_textureBuffer[i]->frameIndex)
			{
				rangeEnd = m_textureBuffer[i]->frameIndex;
			}
		}
		i = ( i + 1 ) % RING_QUEUE_SIZE;
	}

	return frameIndex >= rangeStart && frameIndex <= rangeEnd;
}


bool UElectraVideoTextureHog::IsFrameBeyondCachedRange(int64_t frameIndex) const
{
	int64_t rangeStart = std::numeric_limits<int64>::max();
	int64_t rangeEnd = -1;
	for (int i = m_textureBufferStart; i != m_textureBufferEnd; )
	{
		if (!m_textureBuffer[i]->isUsed)
		{
			if (rangeStart > m_textureBuffer[i]->frameIndex)
			{
				rangeStart = m_textureBuffer[i]->frameIndex;
			}

			if (rangeEnd < m_textureBuffer[i]->frameIndex)
			{
				rangeEnd = m_textureBuffer[i]->frameIndex;
			}
		}
		i = (i + 1) % RING_QUEUE_SIZE;
	}

	return frameIndex > rangeStart && frameIndex > rangeEnd;
}


bool UElectraVideoTextureHog::IsFrameBeforeCachedRange(int64_t frameIndex) const
{
	int64_t rangeStart = std::numeric_limits<int64>::max();
	int64_t rangeEnd = -1;
	for (int i = m_textureBufferStart; i != m_textureBufferEnd; )
	{
		if (!m_textureBuffer[i]->isUsed)
		{
			if (rangeStart > m_textureBuffer[i]->frameIndex)
			{
				rangeStart = m_textureBuffer[i]->frameIndex;
			}

			if (rangeEnd < m_textureBuffer[i]->frameIndex)
			{
				rangeEnd = m_textureBuffer[i]->frameIndex;
			}
		}
		i = (i + 1) % RING_QUEUE_SIZE;
	}

	return frameIndex < rangeStart && frameIndex < rangeEnd;
}


float UElectraVideoTextureHog::GetVideoDuration() const
{
	if (m_mediaOpened && m_mediaPlayer)
	{
		return (float)m_mediaPlayer->GetDuration().GetTotalSeconds();
	}

	return 0;
}
