// Fill out your copyright notice in the Description page of Project Settings.


#include "EvercoastStreamingReaderComp.h"
#include "EvercoastDecoder.h"
#include "CortoDecoder.h"
#include "WebpDecoder.h"
#include "EvercoastBasicStreamingDataDecoder.h"
#include "EvercoastBasicStreamingDataUploader.h"
#include "CortoDataUploader.h"
#include "EvercoastAsyncStreamingDataDecoder.h"
#include "EvercoastRendererSelectorComp.h"
#include "Components/AudioComponent.h"
#include <memory>
#include <map>
#include "ec/reading/API_events.h"
#include "TimestampedMediaTexture.h"
#include "Kismet/GameplayStatics.h"

#include "FFmpegVideoTextureHog.h"
#include "ElectraVideoTextureHog.h"

#include "EvercoastMediaSoundComp.h"
#include "MediaPlayer.h"
#include "CortoWebpUnifiedDecodeResult.h"
#include "RuntimeAudio.h"
#include "EvercoastVolcapActor.h"

static std::map<GTHandle, UEvercoastStreamingReaderComp*> s_readerCompRegistry;

extern UGhostTreeFormatReader* find_reader(GTHandle reader_inst);
static UEvercoastStreamingReaderComp* find_reader_comp(GTHandle reader_inst)
{
	auto it = s_readerCompRegistry.find(reader_inst);
	check(it != s_readerCompRegistry.end());
	if (it != s_readerCompRegistry.end())
	{
		return it->second;
	}

	return nullptr;
}


class TheReaderDelegate
{
public:
	static ReadDelegate get_callbacks_for_c()
	{
		return ReadDelegate
		{
			on_event,
			on_playback_info_received,
			on_meta_data_received,
			on_channels_received,
			//on_next_block_not_ready,

			on_block_received,
			on_block_invalidated,
			on_last_block,

			on_cache_update,
			on_finished_with_cache_id,
			on_free_space_in_cache,

			open_connection,
			read_from_connection,
			cancel_connection_request,
			read_from_cache,
			close_connection
		};
	}

	static void on_event(GTHandle reader_inst, ECReaderEvent event)
	{
		find_reader(reader_inst)->OnReaderEvent(event);
		find_reader_comp(reader_inst)->OnReaderEvent(event);
	}

	static void on_playback_info_received(GTHandle reader_inst, PlaybackInfo playback_info)
	{
		find_reader(reader_inst)->OnPlaybackInfoReceived(playback_info);
	}
	static void on_meta_data_received(GTHandle reader_inst, uint32_t count, const char* keys[], const char* values[])
	{
		find_reader(reader_inst)->OnMetaDataReceived(count, keys, values);
	}
	static void on_channels_received(GTHandle reader_inst, uint32_t count, ChannelInfo* channel_infos)
	{
		find_reader(reader_inst)->OnChannelsReceived(count, channel_infos);
		find_reader_comp(reader_inst)->NotifyReceivedChannelsInfo();
	}

	static void on_next_block_not_ready(GTHandle reader_inst, uint32_t channel_id)
	{
		find_reader(reader_inst)->OnNextBlockNotReady(channel_id);
	}

	static void on_block_received(GTHandle reader_inst, ChannelDataBlock data_block)
	{
		UGhostTreeFormatReader* reader = find_reader(reader_inst);
		reader->OnBlockReceived(data_block);
		// only use main timestamp
		if (data_block.channel_id == reader->GetMainChannelId())
		{
			find_reader_comp(reader_inst)->NotifyReceivedTimestamp((float)data_block.timestamp);
		}
	}
	
	static void on_block_invalidated(GTHandle reader_inst, uint32_t block_id)
	{
		find_reader(reader_inst)->OnBlockInvalidated(block_id);
	}

	static void on_last_block(GTHandle reader_inst, uint32_t channel_id)
	{
	}

	static void on_cache_update(GTHandle reader_inst, double cached_until)
	{
		find_reader(reader_inst)->OnCacheUpdate(cached_until);
	}
	static void on_finished_with_cache_id(GTHandle reader_inst, uint32_t cache_id)
	{
		find_reader(reader_inst)->OnFinishedWithCacheId(cache_id);
	}

	static void on_free_space_in_cache(GTHandle handle, uint32_t cache_id, uint32_t offset, uint32_t size)
	{
	}

	static bool open_connection(GTHandle reader_inst, uint32_t conn_handle, const char* name)
	{
		// avoid shortcircuit
		bool ret1 = find_reader(reader_inst)->OnOpenConnection(conn_handle, name);
		bool ret2 = find_reader_comp(reader_inst)->OnOpenConnection(ret1);
		return ret1 && ret2;
	}
	static bool read_from_connection(GTHandle reader_inst, uint32_t conn_handle, ReadRequest request)
	{ 
		return find_reader(reader_inst)->OnReaderReadFromConnection(conn_handle, request);
	}

	static bool cancel_connection_request(GTHandle handle, uint32_t conn_handle, uint32_t requestId)
	{
		return true;
	}

	static bool read_from_cache(GTHandle handle, ReadRequest request)
	{
		return true;
	}

	static bool close_connection(GTHandle reader_inst, uint32_t conn_handle)
	{ 
		// avoid shortcircuit
		bool ret1 = find_reader(reader_inst)->OnCloseConnection(conn_handle);
		bool ret2 = find_reader_comp(reader_inst)->OnCloseConnection(ret1);
		return ret1 && ret2;
	}
};

// Sets default values for this component's properties
UEvercoastStreamingReaderComp::UEvercoastStreamingReaderComp(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	bAsyncDataDecoding(true),
	bPreferVideoCodec(false),
	bAutoPlay(true),
	bLoop(true),
	bSlackTiming(false),
	Renderer(nullptr),
	RendererActor(nullptr),
	m_currReadTimestamp(0),
	m_playbackStatus(PlaybackStatus::Stopped),
	m_isReaderWaitingForData(true),
	m_isReaderInSeeking(false),
	m_isReaderPlaybackReady(false),
	m_isReaderWaitingForAudioData(true),
	m_isReaderWaitingForVideoData(false),
	m_videoTextureHog(nullptr),
	m_syncStatus(SyncStatus::InSync),
	m_lastMismatchedFrame(-1),
	m_consecutiveMismatchedFrameCount(0),
	m_playbackInitSeek(0),
	m_geomFeedStarvingStartTime(0),
	m_cortoTexSeekStage(CTS_NA),
	m_readerHasFatalError(false),
	m_currentMatchingFrameNumber(0),
	m_currentMatchingTimestamp(0.0f)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	bTickInEditor = true; // kicks off uploading
}

UEvercoastStreamingReaderComp::~UEvercoastStreamingReaderComp()
{
	ResetReader();
}

void UEvercoastStreamingReaderComp::CreateReader()
{
	auto* actor = GetOwner();
	m_audioComponent = actor->FindComponentByClass<UAudioComponent>();
	if (m_audioComponent)
	{
		m_audioComponent->bAutoActivate = false;
		m_audioComponent->SetActive(false);
		m_audioComponent->bAutoDestroy = false;
	}
	m_reader = UGhostTreeFormatReader::Create(GetWorld()->WorldType == EWorldType::Editor, m_audioComponent, MaxCacheSizeInMB);
	m_reader->SetInitialSeek(m_playbackInitSeek);
	s_readerCompRegistry.insert(std::make_pair(m_reader->GetRawHandle(), this));
	m_reader->SetStatusCallbackRaw(this);

	if (DataBitRateLimit == TEXT("Unlimited"))
	{
		m_reader->SetBitRateLimit((uint32_t)-1);
	}
	else
	{
		m_reader->SetBitRateLimit(FCString::Atoi(*DataBitRateLimit) * 1024 * 1024);
	}

	if (DesiredFrameRate == TEXT("Highest"))
	{
		m_reader->SetDesiredFrameRate(0);
	}
	else
	{
		m_reader->SetDesiredFrameRate(FCString::Atoi(*DesiredFrameRate));
	}

	m_reader->SetUsingMemoryCache(bForceMemoryCache);
	m_reader->SetPreferExternalVideoData(bPreferVideoCodec);
	
	if (!ECVAsset)
	{
		UE_LOG(EvercoastReaderLog, Warning, TEXT("Asset is empty. No data will be decoded or rendered, nor the decoder will be created."));
		m_dataDecoder = nullptr;
		m_baseDecoder = nullptr;
		m_auxDecoder = nullptr;

		// No file opened, but still need to keep promise
		m_fileOpenPromise = std::promise<void>();
		m_fileOpenFuture = m_fileOpenPromise.get_future();
		m_fileOpenPromise.set_value();
		return;
	}

	if (ECVAsset->GetDataURL().EndsWith(".ecv"))
	{
		m_baseDecoder = EvercoastDecoder::Create();
	}
	else if (ECVAsset->GetDataURL().EndsWith(".ecm"))
	{
		m_baseDecoder = CortoDecoder::Create();
		m_auxDecoder = WebpDecoder::Create();
	}
	else
	{
		m_dataDecoder = nullptr;
		m_baseDecoder = nullptr;
		m_auxDecoder = nullptr;

		UE_LOG(EvercoastReaderLog, Error, TEXT("Asset suffix is neither .ecv nor .ecm. Cannot choose a decoder."));
		OnFatalError("Cannot Choose Decoder Error");
		return;
	}

	if (bAsyncDataDecoding)
	{
		m_dataDecoder = std::make_shared<EvercoastAsyncStreamingDataDecoder>(m_baseDecoder, m_auxDecoder);
	}
	else
	{
		m_dataDecoder = std::make_shared<EvercoastBasicStreamingDataDecoder>(m_baseDecoder, m_auxDecoder);
	}

	m_fileOpenPromise = std::promise<void>();
	m_fileOpenFuture = m_fileOpenPromise.get_future();
	m_reader->OpenFromLocation(ECVAsset->GetDataURL(), TheReaderDelegate::get_callbacks_for_c(), m_dataDecoder);
	m_reader->Tick();
}

void UEvercoastStreamingReaderComp::ResetReader()
{
	if (m_reader)
	{
        GTHandle readerHandle = m_reader->GetRawHandle();
		m_reader->Close();
		m_reader->SetStatusCallbackRaw(nullptr);
		m_reader = nullptr;

		s_readerCompRegistry.erase(readerHandle);

		if (m_videoTextureHog)
		{
			m_videoTextureHog->Close();
            m_videoTextureHog->Destroy();
            m_videoTextureHog = nullptr;
		}
	}

	m_dataDecoder = nullptr;
	m_baseDecoder = nullptr;
	m_auxDecoder = nullptr;
	m_readerHasFatalError = false;
	m_currentMatchingFrameNumber = 0;
	m_currentMatchingTimestamp = 0.0f;

}

void UEvercoastStreamingReaderComp::OnRegister()
{
	Super::OnRegister();

	if (!GIsCookerLoadingPackage)
	{
		ResetReader();

		CreateReader();

		
	}
}

void UEvercoastStreamingReaderComp::OnUnregister()
{
	Super::OnUnregister();

	if (!GIsCookerLoadingPackage)
	{
		ResetReader();

		m_timestampDriver.Reset();
	}
}

void UEvercoastStreamingReaderComp::ToggleAuxPlaybackIfPossible(bool play)
{
	if (m_audioComponent)
	{
		USoundBase* soundWaveBase = m_audioComponent->Sound;
		if (soundWaveBase)
		{
			if (soundWaveBase->IsA<URuntimeAudio>())
			{
				auto runtimeAudio = static_cast<URuntimeAudio*>(soundWaveBase);
				runtimeAudio->SeekToTime(m_currReadTimestamp);

			}
			else
			{
				// treat as normal sound wave
			}

			
			if (play)
			{
				if (!m_audioComponent->IsPlaying())
				{
					//UE_LOG(EvercoastReaderLog, Log, TEXT("Play Sound"));
					m_audioComponent->Play();
				}

				m_audioComponent->SetPaused(false);
			}
			else
			{
				m_audioComponent->SetPaused(true);
			}
		}
	}

	SetPlaybackTiming(play);
}

void UEvercoastStreamingReaderComp::ToggleVideoTextureHogIfPossible(bool play)
{
	if (m_videoTextureHog && m_baseDecoder && m_baseDecoder->GetType() == DT_CortoMesh)
	{
		check(m_reader->MeshRequiresExternalData());
		if (play)
		{
			m_videoTextureHog->ResetTo(m_currReadTimestamp - m_reader->GetFrameInterval()); // give one frame of clearance
			m_videoTextureHog->StartHogging();
		}
		else {
			m_videoTextureHog->StopHogging();
		}
	}
}

void UEvercoastStreamingReaderComp::SetPlaybackTiming(bool play)
{
	if (m_timestampDriver)
	{
		if (play)
			m_timestampDriver->Start();
		else
			m_timestampDriver->Pause();
	}
}

float UEvercoastStreamingReaderComp::GetPlaybackTiming() const
{
	if (m_timestampDriver)
		return m_timestampDriver->GetElapsedTime();
	return 0;
}

void UEvercoastStreamingReaderComp::SetRendererActor(AEvercoastVolcapActor* InActor)
{
	if (!InActor)
		return;

	auto oldRenderer = Renderer;

	RendererActor = InActor;
	if (RendererActor)
	{
		Renderer = RendererActor->RendererSelector;
		if (Renderer)
		{
			// Make sure it has compatible sub renderer
			if (m_baseDecoder)
				Renderer->ChooseCorrespondingSubRenderer(m_baseDecoder->GetType());
		}
	}
	else
	{
		Renderer = nullptr;
	}
}

void UEvercoastStreamingReaderComp::SetECVAsset(UEvercoastECVAsset* asset)
{
	ECVAsset = asset;
	RecreateReader();
}

AEvercoastVolcapActor* UEvercoastStreamingReaderComp::GetRendererActor() const
{
	return RendererActor;
}

void UEvercoastStreamingReaderComp::RecreateReader()
{
	ResetReader();
	CreateReader();
}

void UEvercoastStreamingReaderComp::RecreateReaderSync()
{
	ResetReader();
	CreateReader();

	ensureMsgf(m_fileOpenFuture.valid(), TEXT("Invalid IO future!"));
	m_fileOpenFuture.get();

	if (m_baseDecoder && m_baseDecoder->GetType() == DT_CortoMesh && m_videoTextureHog)
	{
		check(m_reader && m_reader->MeshRequiresExternalData());
	}
}

void UEvercoastStreamingReaderComp::OnWaitingForDataChanged(bool waitingForData)
{
	m_isReaderWaitingForData = waitingForData;
	if (m_playbackStatus == PlaybackStatus::Playing)
	{
		// trigger audio playback
		ToggleAuxPlaybackIfPossible(IsReaderPlayableWithoutBlocking());
	}
	else if (m_playbackStatus == PlaybackStatus::Paused)
	{
		// take care of video texutre capture while in seeking while in paused state
		ToggleVideoTextureHogIfPossible(IsReaderPlayableWithoutBlocking());
	}
	_PrintDebugStatus();
}


void UEvercoastStreamingReaderComp::OnInSeekingChanged(bool inSeeking)
{
	m_isReaderInSeeking = inSeeking;
	if (m_playbackStatus == PlaybackStatus::Playing)
	{
		// trigger audio playback
		bool canPlay = IsReaderPlayableWithoutBlocking();
		if (inSeeking)
		{
			// flush the decoder's remaining results, if requested seeking
			m_dataDecoder->FlushAndDisposeResults();
		}
		ToggleAuxPlaybackIfPossible(canPlay);
	}
	else if (m_playbackStatus == PlaybackStatus::Paused)
	{
		bool canPlay = IsReaderPlayableWithoutBlocking();
		if (inSeeking)
		{
			// flush the decoder's remaining results, if requested seeking
			m_dataDecoder->FlushAndDisposeResults();
		}
		// only toggle video texture hog
		ToggleVideoTextureHogIfPossible(canPlay);
	}

	if (m_timestampDriver)
	{
		if (inSeeking)
		{
			m_timestampDriver->ResetTimerTo(m_reader->GetSeekingTarget(), m_playbackStatus == PlaybackStatus::Playing);
		}
		else
		{
			m_timestampDriver->ResetTimerTo(m_reader->GetCurrentTimestamp(), m_playbackStatus == PlaybackStatus::Playing);
		}
	}

	_PrintDebugStatus();
}


void UEvercoastStreamingReaderComp::OnPlaybackReadyChanged(bool playbackReady)
{
	m_isReaderPlaybackReady = playbackReady;
	if (m_playbackStatus == PlaybackStatus::Playing)
	{
		// trigger audio playback
		ToggleAuxPlaybackIfPossible(IsReaderPlayableWithoutBlocking());
	}
	else if (m_playbackStatus == PlaybackStatus::Paused)
	{
		ToggleVideoTextureHogIfPossible(IsReaderPlayableWithoutBlocking());
	}
	_PrintDebugStatus();
}

void UEvercoastStreamingReaderComp::OnFatalError(const char* error)
{
	m_readerHasFatalError = true;
	m_readerFataErrorMessage = FString(ANSI_TO_TCHAR(error));
}

void UEvercoastStreamingReaderComp::OnWaitingForAudioDataChanged(bool isWaitingForAudioData)
{
	m_isReaderWaitingForAudioData = isWaitingForAudioData;
	if (m_playbackStatus == PlaybackStatus::Playing)
	{
		ToggleAuxPlaybackIfPossible(IsReaderPlayableWithoutBlocking());
	}
	else if (m_playbackStatus == PlaybackStatus::Paused)
	{
		ToggleVideoTextureHogIfPossible(IsReaderPlayableWithoutBlocking());
	}
	_PrintDebugStatus();
}

void UEvercoastStreamingReaderComp::OnWaitingForVideoDataChanged(bool isWaitingForVideoData)
{
	m_isReaderWaitingForVideoData = isWaitingForVideoData;
	if (m_playbackStatus == PlaybackStatus::Playing)
	{
		ToggleAuxPlaybackIfPossible(IsReaderPlayableWithoutBlocking());
	}
	else if (m_playbackStatus == PlaybackStatus::Paused)
	{
		ToggleVideoTextureHogIfPossible(IsReaderPlayableWithoutBlocking());
	}
	_PrintDebugStatus();
}


bool UEvercoastStreamingReaderComp::IsStreamingDurationReliable() const
{
	if (!m_reader)
		return false;

	if (m_baseDecoder && m_baseDecoder->GetType() == DT_CortoMesh && m_videoTextureHog)
	{
		return m_videoTextureHog->IsVideoOpened();
	}

	return true;
}

void UEvercoastStreamingReaderComp::WaitForDurationBecomesReliable()
{
	if (m_baseDecoder && m_baseDecoder->GetType() == DT_CortoMesh && m_videoTextureHog)
	{
		while (!m_videoTextureHog->IsVideoOpened())
		{
			m_videoTextureHog->Tick(GetWorld());
		}
	}

	return;
}

bool UEvercoastStreamingReaderComp::IsReaderPlayableWithoutBlocking() const
{
	return m_isReaderPlaybackReady && !m_isReaderInSeeking && !m_isReaderWaitingForData && !m_isReaderWaitingForAudioData && !m_isReaderWaitingForVideoData;
}

void UEvercoastStreamingReaderComp::_PrintDebugStatus() const
{
	UE_LOG(EvercoastReaderLog, Verbose, TEXT("PlaybackReady: %d\tInSeeking: %d\tWaitingForData: %d\tWaitingForAudioData: %d\tWaitingForVideoData: %d"), 
		m_isReaderPlaybackReady, m_isReaderInSeeking, m_isReaderWaitingForData, m_isReaderWaitingForAudioData, m_isReaderWaitingForVideoData);

}

// Called when the game starts
void UEvercoastStreamingReaderComp::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoPlay)
	{
		StreamingPlay();
	}
}

void UEvercoastStreamingReaderComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UEvercoastStreamingReaderComp::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FName propertyChanged = PropertyChangedEvent.GetPropertyName();
	if (propertyChanged == FName(TEXT("RendererActor")))
	{
		// force calling RenderActor setter
		SetRendererActor(RendererActor);
	}
	else if (propertyChanged == FName(TEXT("ECVAsset")))
	{
		// create or delete the renderer according to settings
		DoRefreshRenderer();
	}

	// Always calls super impl at last! Otherwise weird garbage collection will happen and the change will never make into the component
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UEvercoastStreamingReaderComp::OnReaderEvent(ECReaderEvent event)
{
	if (event == EC_STREAMING_EVENT_PLAYBACK_READY && m_audioComponent)
	{
	}
}

void UEvercoastStreamingReaderComp::NotifyReceivedTimestamp(float timeStamp)
{
	m_currReadTimestamp = timeStamp;

	if (m_syncStatus == SyncStatus::VideoFeedStarving ||
		m_syncStatus == SyncStatus::InSync_SkippedFrames)
	{
		// when video player is starving for geom or video player has skipped frames,
		// geometry data will be forced to skip. In this case, we need to synchronise 
		// the playback timer to the geometry data timestamp. This will cause inaccurate
		// timing but there's no better way of get around it
        m_timestampDriver->ResetTimerTo(timeStamp, m_playbackStatus == PlaybackStatus::Playing);
	}
}

void UEvercoastStreamingReaderComp::NotifyReceivedChannelsInfo()
{
	FVector origin;
	FQuat orientation;
	m_reader->GetChannelSpatialInfo(origin, orientation);

	if (Renderer)
	{
		Renderer->SetRelativeLocationAndRotation(origin, orientation);
	}

	// By the time ghost tree reader should already get the "corrected" clip duration
	m_timestampDriver = MakeShared<FTimestampDriver, ESPMode::NotThreadSafe>(m_reader->GetDuration());
	
	if (m_baseDecoder && m_baseDecoder->GetType() == DT_CortoMesh)
	{
		check(m_reader->IsMeshData());

		m_dataDecoder->SetRequiresExternalData(m_reader->MeshRequiresExternalData());
		if (m_reader->MeshRequiresExternalData())
		{
			bool ret = true;
			// Mesh needs a lot of auxiliary objects, but info only available after received channel infos
			FString url = ECVAsset->GetDataURL();
			// should only replace the last ".ecm"
			FString videoUrl(url);
			if (videoUrl.RemoveFromEnd(TEXT(".ecm"), ESearchCase::IgnoreCase))
			{
				videoUrl.Append(m_reader->GetExternalPostfix());
			}
			else
			{
				UE_LOG(EvercoastReaderLog, Warning, TEXT("Abnormal ecm url: %s"), *url);
			}

			if (!m_videoTextureHog)
			{
				if (VideoCodecProvider == EVideoCodecProvider::BUILTIN)
				{
					FName name = MakeUniqueObjectName((UObject*)GetTransientPackage(), UElectraVideoTextureHog::StaticClass(), FName(this->GetName() + TEXT("_VideoTextureHog")));
					m_videoTextureHog = NewObject<UElectraVideoTextureHog>((UObject*)GetTransientPackage(), name);

					UE_LOG(EvercoastReaderLog, Log, TEXT("%s using built-in decoder."), *UGameplayStatics::GetPlatformName());
				}
				else
				{

					FName name = MakeUniqueObjectName((UObject*)GetTransientPackage(), UFFmpegVideoTextureHog::StaticClass(), FName(this->GetName() + TEXT("_VideoTextureHog")));
					m_videoTextureHog = NewObject<UFFmpegVideoTextureHog>((UObject*)GetTransientPackage(), name);

					UE_LOG(EvercoastReaderLog, Log, TEXT("%s using ffmpeg decoder."), *UGameplayStatics::GetPlatformName());
				}
			}

			m_cortoTexSeekStage = CTS_DEFAULT;

			if (ECVAsset->IsHttpStreaming())
			{
				ret = m_videoTextureHog->OpenUrl(videoUrl);
			}
			else
			{
				FString dummyVideoUrl = FString(videoUrl);

				FString fullVideoPath;
				bool isRelative = FPaths::IsRelative(dummyVideoUrl);
				if (isRelative)
				{
					fullVideoPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), videoUrl));
				}
				else
				{
					fullVideoPath = videoUrl;
				}

				ret = m_videoTextureHog->OpenFile(fullVideoPath);
			}

			if (!ret)
			{
				UE_LOG(EvercoastReaderLog, Error, TEXT("Opening video file failed: %s -> %s"), *url, *videoUrl);
			}
			else
			{
				if (m_videoTextureHog->IsVideoOpened())
				{
					float videoDuration = m_videoTextureHog->GetVideoDuration();
					if (videoDuration > 0 && videoDuration < m_reader->GetDuration())
					{
						// force change the video duration as we found a better one
						m_timestampDriver->ForceChangeVideoDuration(videoDuration);
					}
				}
			}
		}
		else
		{
			UE_LOG(EvercoastReaderLog, Log, TEXT("No external data needed for %s"), *ECVAsset->GetDataURL());
			m_cortoTexSeekStage = CTS_NA;
		}
	}
	else
	{
		m_cortoTexSeekStage = CTS_NA;
	}

	if (m_reader->IsAudioDataAvailable())
	{
		m_timestampDriver->UseAudioTimestamps(m_reader->GetReceivingAudioComponent());
	}
	else
	{
		m_timestampDriver->UseWorldTimestamps(GetWorld());
	}
	m_fileOpenPromise.set_value();
}

bool UEvercoastStreamingReaderComp::OnOpenConnection(bool prerequisitySucceeded)
{
	if (!prerequisitySucceeded)
	{
		// keep promise
		m_fileOpenPromise.set_value();
		return false;
	}

	bool ret = true;

	if (m_baseDecoder->GetType() == DT_CortoMesh ||
		m_baseDecoder->GetType() == DT_EvercoastVoxel)
	{
		DoRefreshRenderer();

	}
	else
	{
		ensureMsgf(false, TEXT("Unknown decoder type: %d. Cannot create renderer."), (int)m_baseDecoder->GetType());
		// keep promise
		m_fileOpenPromise.set_value();
		ret = false;
	}

	return ret;
}

void UEvercoastStreamingReaderComp::DoRefreshRenderer()
{
	if (!m_baseDecoder)
		return;

	if (Renderer)
		Renderer->ChooseCorrespondingSubRenderer(m_baseDecoder->GetType());
}

bool UEvercoastStreamingReaderComp::OnCloseConnection(bool prerequisitySucceeded)
{
	if (m_baseDecoder->GetType() == DT_CortoMesh)
	{
		if (m_videoTextureHog)
		{
			m_videoTextureHog->Close();
            m_videoTextureHog->Destroy();
            m_videoTextureHog = nullptr;
		}

	}

	return true;
}

// Called every frame
void UEvercoastStreamingReaderComp::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	constexpr float playbackStartTime = 0.0f; // should rewind to 0, the very beginning
	bool shouldFreezeFirstFrame = (GetWorld()->WorldType == EWorldType::Editor);

	bool isPlayingOrPause = (m_playbackStatus == PlaybackStatus::Playing) || (m_playbackStatus == PlaybackStatus::Paused);
	bool isPlaying = m_playbackStatus == PlaybackStatus::Playing;

	float dueTimestamp = GetPlaybackTiming();
	bool scrutinizeTime = false;

	bool needDataUploading = true;

	if (m_audioComponent && m_audioComponent->Sound && m_audioComponent->Sound->IsA<URuntimeAudio>())
	{
		scrutinizeTime = true;
	}
	else if (m_timestampDriver && m_timestampDriver->IsSequencerOverriding())
	{
		scrutinizeTime = true;
	}

	if (!isPlaying)
		scrutinizeTime = false; //  no need to scrutinize time when paused

	// Override scrutinizeTime
	if (bSlackTiming)
		scrutinizeTime = false;
	//////////////////////////////////////////////////////////////
	// Delta time calculation, micro time management and looping
	if (m_dataDecoder && m_baseDecoder
		&& isPlayingOrPause
		&& m_currReadTimestamp >= 0
		&& !m_reader->IsInSeeking()
		)
	{
		// Still need to deal with the special case for geometry feed starving
		if (m_videoTextureHog && m_syncStatus == SyncStatus::GeomFeedStarving
			&& !scrutinizeTime)
		{
			check(m_reader->MeshRequiresExternalData());
			// Do not advance timestamp unless video player has reached the end.
			if (!m_videoTextureHog->IsEndReached())
			{
				dueTimestamp = m_geomFeedStarvingStartTime;
			}
			else
			{
				// If the video has reached the end, that means there won't be 
				// any valid video frame coming out(video texture hog doesn't automatically loop), we do 
				// want to advance the timestamp so that the playback will hit the end then loop to the start.
				// This is not ideal but since some videos has large keyframe gaps, it happens when video
				// seek to a point close enough to the last keyframe, and the seeking algorithm does pin
				// the seeking to that last keyframe, it will 'drag' the playback there and soon will reach
				// the end.
			}
		}
		
		// In the middle of looping(only cares when ECM with external video), keep the dueTimestamp zero
		bool loopInProgress = m_cortoTexSeekStage == CTS_REQUESTED || m_cortoTexSeekStage == CTS_COMPLETED;
		if (loopInProgress)
		{
			dueTimestamp = playbackStartTime;
		}
		else // Do not request more frames if in the middle of looping, otherwise will increase the discrepency of timestamp of mesh/texture
		{

			if (dueTimestamp - m_currReadTimestamp >= m_reader->GetFrameInterval())
			{
				if (!shouldFreezeFirstFrame || m_timestampDriver->IsSequencerOverriding())
				{
					m_reader->RequestFrameNext();
				}
			}
		}

		float duration;
		if (m_videoTextureHog && m_videoTextureHog->IsVideoOpened() && m_baseDecoder->GetType() == DT_CortoMesh)
		{
			check(m_reader->MeshRequiresExternalData());
			duration = std::min(m_reader->GetDuration(), m_videoTextureHog->GetVideoDuration());

			// also check the timestamp driver, esp when geometry duration mismatch with video duration
			// This normally should be done in NotifyChannelInfo() however due to streaming assets' async requirements
			// it has to be done here for a non-blocking experience

			if (duration > 0 && m_timestampDriver->GetVideoDuration() > duration)
			{
				// force changing video duration as we found a better one
				m_timestampDriver->ForceChangeVideoDuration(duration);
			}

		}
		else
		{
			duration = m_reader->GetDuration();
		}

		UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Due time: %.3f, Read time: %.3f, playback timer: %.3f, duration: %.3f geom feed starving time: %.3f"), dueTimestamp, m_currReadTimestamp, GetPlaybackTiming(), duration, m_geomFeedStarvingStartTime);

		bool videoHitEOF = false;
		if (m_videoTextureHog && m_videoTextureHog->IsEndReached())
			videoHitEOF = true;

		bool isInSequencer = (m_timestampDriver && m_timestampDriver->IsSequencerOverriding());

		if ((bLoop || isInSequencer) && duration > 0 && ((dueTimestamp >= duration) || loopInProgress || videoHitEOF))
		{
			
			constexpr float seekingTargetDiffThreshold = 1.0f / 10.0f;
			float timeDiff = FMath::Abs(m_reader->GetCurrentTimestamp() - playbackStartTime);
			bool currTimestampCloseEnough = timeDiff < seekingTargetDiffThreshold;

			if (!currTimestampCloseEnough)
			{
				if (m_videoTextureHog && m_baseDecoder->GetType() == DT_CortoMesh)
				{
					// The reason we want to do media player seeking first, is on Android the seeking process is slow and will lag
					// till the mesh buffer is full. This will create a forever mismatch of mesh-texture pair. So here we force to
					// seek media player first.
					if (m_cortoTexSeekStage == CTS_DEFAULT)
					{
						// initiate media player seeking request
						m_cortoTexSeekStage = CTS_REQUESTED;
						// reset sync status
						m_syncStatus = SyncStatus::InSync;

						m_videoTextureHog->ResetTo(playbackStartTime, [self=this, playbackStartTime]() {
								UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("VideoTextureHog Seek to %.2f completed"), playbackStartTime);
								self->m_cortoTexSeekStage = CTS_COMPLETED; 
								self->OnWaitingForVideoDataChanged(false); // force clean the wait for video flag
							}
						);
					}
					else if (m_cortoTexSeekStage == CTS_COMPLETED)
					{

						if (!(m_reader->GetSeekingTarget() == playbackStartTime && m_reader->IsInSeeking()))
						{
							// check curr timestamp too so avoid repeatedly request seeking(==infinite seeking and heavy IO)...
							m_reader->RequestFrameOnTimestamp(playbackStartTime);
						}

						m_cortoTexSeekStage = CTS_DEFAULT;
					}
				}
				else
				{
					if (!(m_reader->GetSeekingTarget() == playbackStartTime && m_reader->IsInSeeking()))
					{
						// check curr timestamp too so avoid repeatedly request seeking(==infinite seeking and heavy IO)...
						m_reader->RequestFrameOnTimestamp(playbackStartTime);
					}
				}
			}


			if (currTimestampCloseEnough)
			{
				// when the seeking is successful(almost)
				if (IsReaderPlayableWithoutBlocking())
				{
					// force audio to rewind to playbackStartTime
					m_currReadTimestamp = playbackStartTime;
					// flush the decoder's remaining result. This needs to be done
					// for corto mesh/video decoder because the mismatch num of video frames
					// and geo frames and can cause delays
					if (m_baseDecoder->GetType() == DT_CortoMesh)
					{
						m_dataDecoder->FlushAndDisposeResults();
						m_syncStatus = SyncStatus::InSync; // reset sync status
					}

					// Mark loop for any driver that needs it
					m_timestampDriver->MarkLoop();
					// force reset timer
					m_timestampDriver->ResetTimer();

					ToggleAuxPlaybackIfPossible(true);

					
				}
			}
			
			// when we loop back to the beginning, we don't want to meddling with geometry/mesh sync status
			needDataUploading = false;
		}
	}
	//////////////////////////////////////////

	//////////////////////////////////////////
	// Deal with visibility when stopped
	bool needRendering = true;
	if (m_playbackStatus == PlaybackStatus::Stopped)
	{
		if (Renderer && !Renderer->bKeepRenderedFrameWhenStopped)
		{
			needRendering = false;
		}
	}

	if (Renderer && needRendering != Renderer->IsVisible())
	{
		Renderer->SetVisibility(needRendering, true);
	}
	//////////////////////////////////////////


	/////////////////////////////////////////
	// Data uploading
	if (m_dataDecoder && needDataUploading)
	{
		
		if (m_baseDecoder->GetType() == DT_EvercoastVoxel)
		{
			if (Renderer)
			{
				auto uploader = Renderer->GetDataUploader();
				if (uploader)
				{
					auto result = m_dataDecoder->PopResult();
					uploader->Upload(result.get());

					if (result && result->DecodeSuccessful)
					{
						m_currentMatchingFrameNumber = result->frameIndex;
						m_currentMatchingTimestamp = result->frameTimestamp;
					}

					// manually release the result from decoder
					m_dataDecoder->DisposeResult(std::move(result));
				}
			}
		}
		else if (m_baseDecoder->GetType() == DT_CortoMesh)
		{
			if (Renderer)
			{
				auto uploader = Renderer->GetDataUploader();
				auto result = m_dataDecoder->PeekResult();
				if (uploader && result->DecodeSuccessful)
				{
					// When external video data is required
					if (m_reader->MeshRequiresExternalData())
					{
						if (scrutinizeTime)
						{

							// find the closest candidate, this will improve the time matching of final rendered pictures, but will 
							// drastically reduce the chance finding a pair of matching geom/video frame
							auto nextResult = m_dataDecoder->PeekResult(1);
							do
							{
								result = m_dataDecoder->PeekResult();
								float timeDiff1 = std::max(0.0, dueTimestamp - result->frameTimestamp);
								nextResult = m_dataDecoder->PeekResult(1);
								float timeDiff2 = std::max(0.0, dueTimestamp - nextResult->frameTimestamp);
								if (result->DecodeSuccessful && nextResult->DecodeSuccessful && timeDiff2 < timeDiff1)
								{
									// then that's a better candidate

									// dispose the top result, 
									m_dataDecoder->DisposeResult(m_dataDecoder->PopResult());
									// try comparing the next result<->next next result
								}
								else
								{
									break;
								}
							} while (true);
						}

						if (result->DecodeSuccessful)
						{
							float diff = dueTimestamp - result->frameTimestamp;
							UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Time diff: %.3f at frame: %lld"), diff, result->frameIndex);

							// this should be true most of the time, but it can be false when threaded decoding
							// changes the internal status of the decoder, during the two peeks. Chances are slim tho

							
							check(result->GetType() == DRT_CortoMesh_WebpImage_Unified);
							auto pResult = std::static_pointer_cast<CortoWebpUnifiedDecodeResult>(result);

							UTexture* decodedTexture = FindVideoTexture(pResult->frameIndex);
							if (decodedTexture)
							{
								UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Decoded uploaded at frame: %d, timestamp: %.3f, dueTime: %.3f"), pResult->frameIndex, pResult->frameTimestamp, dueTimestamp);

								m_syncStatus = SyncStatus::InSync;

								m_currentMatchingFrameNumber = pResult->frameIndex;
								m_currentMatchingTimestamp = pResult->frameTimestamp;

								//hitchhike the payload
								pResult->videoTextureResult = decodedTexture;
								m_dataDecoder->PopResult();
								// Uploader and decoder will have to properly handle the lock/unlock of the decoded result,
								// as the results are all preallocated and cached and reusable.

								uploader->Upload(pResult.get());
								m_dataDecoder->DisposeResult(std::move(result));
								InvalidateVideoTexture(decodedTexture);

								// Guard before we call OnWaitingForVideoDataChanged otherwise audio keeps seeking
								if (m_isReaderWaitingForVideoData)
									OnWaitingForVideoDataChanged(false);

								// Try to keep the timing difference as a minimum
								if (scrutinizeTime && diff > 0.2f)
								{
									m_reader->RequestFrameNext();
								}
							}
							else
							{
								if (m_lastMismatchedFrame != pResult->frameIndex)
								{
									UE_LOG(EvercoastReaderLog, Warning, TEXT("Cannot find video texture at frame: %d"), pResult->frameIndex);

									if (m_lastMismatchedFrame + 1 == pResult->frameIndex)
									{
										m_consecutiveMismatchedFrameCount++;
									}
									else
									{
										m_consecutiveMismatchedFrameCount = 0;
									}
									m_lastMismatchedFrame = pResult->frameIndex;
								}

								// Check if the requested index falls within the video texture hog's cache.
								// If it is not, then just wait a few more frames to allow the hog to catch up.
								// If it is, then that means the hog failed to grab the frame, due to video decoder's 
								// jumping frames, so we should just give up this request

								int64_t mismatchedFrame = pResult->frameIndex;
								bool withinRange = IsVideoFrameWithinHogCache(mismatchedFrame);
								if (withinRange)
								{
									if (m_syncStatus != SyncStatus::InSync_SkippedFrames)
										UE_LOG(EvercoastReaderLog, Log, TEXT("Video player skipped the frame. Skipping the mesh frame altogether."));

									m_syncStatus = SyncStatus::InSync_SkippedFrames;
									OnWaitingForVideoDataChanged(true);

									m_dataDecoder->PopResult();
									m_dataDecoder->DisposeResult(std::move(result));

									// request next geom frame till we find a matching one
									m_reader->RequestFrameNext();

									// since we give up we should free up the video hog's slots that is before requested index and restart hogging again, if needed
									InvalidateVideoTextureBeforeFrameIndex(mismatchedFrame);
								}
								else
								{
									// if the video frame hasn't catch up, then we'll just wait wait wait
									bool beyondRange = IsVideoFrameBeyondHogCache(mismatchedFrame);
									if (beyondRange)
									{
										// Check if video texture hog is full. 
										// Full hog will never cache more frames no matter how long we wait. Reset the hog and wait again.
										if (IsVideoTextureHogFull())
										{
											UE_LOG(EvercoastReaderLog, Warning, TEXT("Video player cache is full. Invalidate all already hogged frames then wait."));
											m_videoTextureHog->InvalidateAllTextures();
										}
										else
										{
											// do nothing
											if (m_syncStatus != SyncStatus::GeomFeedStarving)
												UE_LOG(EvercoastReaderLog, Log, TEXT("Video player didn't catch up. Just need to wait"));

											if (scrutinizeTime)
											{
												// maybe hopping a few milliseconds every 5 missing frames, sometimes the video player never catches up..
												// This jump interval is arbitrary now, ideally it should be the length of keyframe interval
												if (m_consecutiveMismatchedFrameCount >= 5)
												{
													UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Jump forward 0.5s to catch up"));
													m_videoTextureHog->JumpBy(0.5f);
													m_consecutiveMismatchedFrameCount = 0;
												}
											}
											// when doing scrubbing, it's likely texture hog is not full but it stopped hogging due to becoming full before scrubbing
											if (!m_videoTextureHog->IsHogging() && m_videoTextureHog->IsHoggingPausedDueToFull() && m_videoTextureHog->IsFrameIndexWithinDuration(mismatchedFrame))
											{
												bool restartedHogging = m_videoTextureHog->StartHogging();
												UE_LOG(EvercoastReaderLog, Warning, TEXT("Video player hogging restarted result: %d"), restartedHogging);
											}
										}

										if (m_syncStatus != SyncStatus::GeomFeedStarving)
										{
											m_syncStatus = SyncStatus::GeomFeedStarving;
											m_geomFeedStarvingStartTime = m_timestampDriver->GetElapsedTime();
											// Geom is waiting for video frames
											OnWaitingForVideoDataChanged(true);
										}
									}
									else
									{
										bool beforeRange = IsVideoFrameBeforeHogCache(mismatchedFrame);
										if (beforeRange)
										{
											// if the cached video frame already passed what's requested, we should just give up this request
											// and at the same time, fast-forward geom feed by keep requesting next frame

											if (m_syncStatus != SyncStatus::VideoFeedStarving)
												UE_LOG(EvercoastReaderLog, Log, TEXT("Video player starving. Skipping the mesh frame ASAP."));

											if (scrutinizeTime)
											{
												// maybe hopping a few milliseconds every 5 missing frames, sometimes the video player never catches up..
												// This jump interval is arbitrary now, ideally it should be the length of keyframe interval
												if (m_consecutiveMismatchedFrameCount >= 5)
												{
													UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Jump backwards 0.5s to catch up"));
													m_videoTextureHog->JumpBy(-0.5f);
													m_consecutiveMismatchedFrameCount = 0;
												}
											}

											m_syncStatus = SyncStatus::VideoFeedStarving;

											m_dataDecoder->PopResult();
											m_dataDecoder->DisposeResult(std::move(result));

											// request next geom frame till we find a matching one
											m_reader->RequestFrameNext();
										}
										else
										{
											// neither before or beyond, or within range, that means the video buffer is empty/all invalid, need to wait
											if (m_syncStatus != SyncStatus::GeomFeedStarving)
												UE_LOG(EvercoastReaderLog, Log, TEXT("Video player's cache is empty. Need to wait"));

											// when doing scrubbing, it's likely texture hog is not full but it stopped hogging due to becoming full before scrubbing
											if (!m_videoTextureHog->IsHogging() && m_videoTextureHog->IsHoggingPausedDueToFull() && m_videoTextureHog->IsFrameIndexWithinDuration(mismatchedFrame))
											{
												bool restartedHogging = m_videoTextureHog->StartHogging();
												UE_LOG(EvercoastReaderLog, Warning, TEXT("Video player hogging restarted result: %d"), restartedHogging);
											}

											if (m_syncStatus != SyncStatus::GeomFeedStarving)
											{
												m_syncStatus = SyncStatus::GeomFeedStarving;
												m_geomFeedStarvingStartTime = m_timestampDriver->GetElapsedTime();
												// Geom is waiting for video frames
												OnWaitingForVideoDataChanged(true);
											}
										}
									}
								}
							}
						}
						else
						{
							UE_LOG(EvercoastReaderLog, Warning, TEXT("Filtering candidates caused unsuccessful result. Decoder changed status during the calls? Frame: %lld, Time: %.3f"), result->frameIndex, dueTimestamp);
						}
					}
					else
					{
						// Texture data from ghosttree container
						check(result->GetType() == DRT_CortoMesh_WebpImage_Unified)
						{
							std::shared_ptr<CortoWebpUnifiedDecodeResult> unifiedResult = std::static_pointer_cast<CortoWebpUnifiedDecodeResult>(result);

							auto meshResult = unifiedResult->meshResult;
							auto imgResult = unifiedResult->imgResult;
							check(meshResult->DecodeSuccessful && imgResult->DecodeSuccessful && meshResult->frameIndex == imgResult->frameIndex);
							check(meshResult->GetType() == DRT_CortoMesh && imgResult->GetType() == DRT_WebpImage);

							uploader->Upload(unifiedResult.get());

							m_dataDecoder->DisposeResult(m_dataDecoder->PopResult());

							m_currentMatchingFrameNumber = unifiedResult->frameIndex;
							m_currentMatchingTimestamp = unifiedResult->frameTimestamp;
						}
						
					}
				}
				else
				{
					// UE_LOG(EvercoastReaderLog, Warning, TEXT("Data uploader null or decode unsuccessful. Time: %.3f"), m_dueTimestamp);
				}
				
				
			}
		}
		else
		{
			// just dispose whatever was decoded
			m_dataDecoder->DisposeResult(m_dataDecoder->PopResult());
		}
	}
	/////////////////////////////////////////
	
	/////////////////////////////////////////
	// Misc components tick
	if (m_reader)
		m_reader->Tick();

	if (m_videoTextureHog)
		m_videoTextureHog->Tick(GetWorld());
	/////////////////////////////////////////
}

UTexture* UEvercoastStreamingReaderComp::FindVideoTexture(int64_t frameIndex)
{
	check(m_videoTextureHog);
	// find video texture from video texture hog
	return m_videoTextureHog->QueryTextureAtIndex(frameIndex);
}

bool UEvercoastStreamingReaderComp::InvalidateVideoTexture(UTexture* pTex)
{
	check(m_videoTextureHog);
	return m_videoTextureHog->InvalidateTextureAndBefore(pTex);
}

bool UEvercoastStreamingReaderComp::InvalidateVideoTextureBeforeFrameIndex(int64_t frameIndex)
{
	check(m_videoTextureHog);
	return m_videoTextureHog->InvalidateTextureAndBeforeByFrameIndex(frameIndex);
}

bool UEvercoastStreamingReaderComp::IsVideoFrameWithinHogCache(int64_t frameIndex)
{
	check(m_videoTextureHog);
	return m_videoTextureHog->IsFrameWithinCachedRange(frameIndex);
}

bool UEvercoastStreamingReaderComp::IsVideoFrameBeyondHogCache(int64_t frameIndex)
{
	check(m_videoTextureHog);
	return m_videoTextureHog->IsFrameBeyondCachedRange(frameIndex);
}

bool UEvercoastStreamingReaderComp::IsVideoFrameBeforeHogCache(int64_t frameIndex)
{
	check(m_videoTextureHog);
	return m_videoTextureHog->IsFrameBeforeCachedRange(frameIndex);
}

bool UEvercoastStreamingReaderComp::IsVideoTextureHogFull() const
{
	check(m_videoTextureHog);
	return m_videoTextureHog->IsFull();
}

void UEvercoastStreamingReaderComp::StreamingPlay()
{
	if (m_playbackStatus == PlaybackStatus::Stopped)
	{
		if (!m_dataDecoder || !m_reader)
		{
			ResetReader();
			CreateReader();
		}
		m_playbackStatus = PlaybackStatus::Playing;
		if (IsReaderPlayableWithoutBlocking())
			ToggleAuxPlaybackIfPossible(true);
	}
	else if (m_playbackStatus == PlaybackStatus::Paused)
	{
		// will be the same as calling resume
		StreamingResume();
	}
}

void UEvercoastStreamingReaderComp::StreamingSeekTo(float timestamp)
{
	if (!m_reader)
		return;

	if (m_playbackStatus == PlaybackStatus::Playing || 
		m_playbackStatus == PlaybackStatus::Paused)
	{
		
		if (m_reader->GetSeekingTarget() != timestamp) // protect reader+videotexturehog from excessive seeking request which never give mediaplayer a break
		{
			m_reader->RequestFrameOnTimestamp(timestamp);

			// Request video texture hogging too (we don't have other means to sync the video hogger, and ToggleVideoTextureHogIfPossible() is not for this purpose)
			if (m_videoTextureHog && m_baseDecoder && m_baseDecoder->GetType() == DT_CortoMesh)
			{
				check(m_reader->MeshRequiresExternalData());
				m_videoTextureHog->ResetTo(timestamp - m_reader->GetFrameInterval()); // give one frame of clearance
				if (!m_videoTextureHog->IsHogging())
					m_videoTextureHog->StartHogging();
			}
		}
	}
}

void UEvercoastStreamingReaderComp::RecalcequencerOverrideLoopCount()
{
	if (m_timestampDriver)
	{
		m_timestampDriver->RecalcSequencerTimestampOverrideLoopCount();
	}
}

void UEvercoastStreamingReaderComp::StreamingJump(float deltaTime)
{
	if (!m_reader)
		return;

	if (m_playbackStatus == PlaybackStatus::Playing || 
		m_playbackStatus == PlaybackStatus::Paused)
	{
		float timestamp = m_reader->GetSeekingTarget() + deltaTime;

		if (m_reader->GetSeekingTarget() != timestamp) // protect reader+videotexturehog from excessive seeking request which never give mediaplayer a break
		{
			m_reader->RequestFrameOnTimestamp(timestamp);

			// Request video texture hogging too (we don't have other means to sync the video hogger, and ToggleVideoTextureHogIfPossible() is not for this purpose)
			if (m_videoTextureHog && m_baseDecoder && m_baseDecoder->GetType() == DT_CortoMesh)
			{
				check(m_reader->MeshRequiresExternalData());
				m_videoTextureHog->ResetTo(timestamp - m_reader->GetFrameInterval()); // give one frame of clearance
				if (!m_videoTextureHog->IsHogging())
					m_videoTextureHog->StartHogging();
			}
		}
	}
}

void UEvercoastStreamingReaderComp::StreamingPrevFrame()
{
	if (!m_reader)
		return;


	StreamingJump(-m_reader->GetFrameInterval());
}

void UEvercoastStreamingReaderComp::StreamingNextFrame()
{
	if (!m_reader)
		return;

	m_reader->RequestFrameNext();
}

float UEvercoastStreamingReaderComp::StreamingGetCurrentTimestamp() const
{
	if (!m_reader)
		return 0.0f;

	if (m_playbackStatus == PlaybackStatus::Playing || 
		m_playbackStatus == PlaybackStatus::Paused)
	{
		return m_currentMatchingTimestamp;
	}

	return 0.0f;
}


int32 UEvercoastStreamingReaderComp::StreamingGetCurrentFrameNumber() const
{
	if (!m_reader)
		return 0;

	if (m_playbackStatus == PlaybackStatus::Playing ||
		m_playbackStatus == PlaybackStatus::Paused)
	{
		return m_currentMatchingFrameNumber;
	}

	return 0;
}


float UEvercoastStreamingReaderComp::StreamingGetCurrentSeekingTarget() const
{
	if (!m_reader)
		return 0.0f;

	if (m_playbackStatus == PlaybackStatus::Playing ||
		m_playbackStatus == PlaybackStatus::Paused)
	{
		return m_reader->GetSeekingTarget();
	}

	return 0.0f;
}

float UEvercoastStreamingReaderComp::StreamingGetDuration() const
{
	if (!m_reader)
		return 0.0f;

	float duration;
	if (m_baseDecoder && m_baseDecoder->GetType() == DT_CortoMesh && m_videoTextureHog)
	{
		check(m_reader->MeshRequiresExternalData());
		duration = std::min(m_reader->GetDuration(), m_videoTextureHog->GetVideoDuration());
	}
	else if (m_reader)
	{
		duration = m_reader->GetDuration();
	}
	else
	{
		duration = 0;
	}

	return duration;
}


void UEvercoastStreamingReaderComp::StreamingResume()
{
	if (m_playbackStatus == PlaybackStatus::Paused)
	{
		m_playbackStatus = PlaybackStatus::Playing;
		if (IsReaderPlayableWithoutBlocking())
			ToggleAuxPlaybackIfPossible(true);
	}
}


void UEvercoastStreamingReaderComp::StreamingPause()
{
	if (m_playbackStatus == PlaybackStatus::Playing)
	{
		m_playbackStatus = PlaybackStatus::Paused;
		ToggleAuxPlaybackIfPossible(false);
	}
}


void UEvercoastStreamingReaderComp::StreamingStop()
{
	ResetReader();

	m_playbackStatus = PlaybackStatus::Stopped;
	ToggleAuxPlaybackIfPossible(false);

	// delete timer
	m_timestampDriver.Reset();
}

bool UEvercoastStreamingReaderComp::IsStreamingPlaying() const
{
	return m_playbackStatus == PlaybackStatus::Playing;
}

bool UEvercoastStreamingReaderComp::IsStreamingPaused() const
{
	return m_playbackStatus == PlaybackStatus::Paused;
}

bool UEvercoastStreamingReaderComp::IsStreamingStopped() const
{
	return m_playbackStatus == PlaybackStatus::Stopped;
}

bool UEvercoastStreamingReaderComp::IsStreamingPendingSeek() const
{
	if (m_playbackStatus == PlaybackStatus::Playing || 
		m_playbackStatus == PlaybackStatus::Paused)
	{
		return m_reader->IsInSeeking();
	}
	return false;
}

bool UEvercoastStreamingReaderComp::IsStreamingWaitingForData() const
{
	if (m_playbackStatus == PlaybackStatus::Playing)
	{
		return m_reader->IsWaitingForData();
	}
	return false;
}

bool UEvercoastStreamingReaderComp::IsStreamingWaitingForAudioData() const
{
	if (m_playbackStatus == PlaybackStatus::Playing)
	{
		return m_reader->IsWaitingForAudioData();
	}
	return false;
}

bool UEvercoastStreamingReaderComp::IsStreamingPlaybackReady() const
{
	if (m_playbackStatus == PlaybackStatus::Playing ||
		m_playbackStatus == PlaybackStatus::Paused)
	{
		return m_reader->IsPlaybackReady();
	}

	return false;
}

bool UEvercoastStreamingReaderComp::IsStreamingPlayableWithoutBlocking() const
{
	return IsStreamingPlaybackReady() && !IsStreamingPendingSeek() && !IsStreamingWaitingForData() && !IsStreamingWaitingForAudioData();
}

void UEvercoastStreamingReaderComp::SetPlaybackStartTime(float startTimestamp)
{
	m_playbackInitSeek = startTimestamp;
	if (m_reader)
	{
		m_reader->SetInitialSeek(startTimestamp);
	}
}

float UEvercoastStreamingReaderComp::GetPlaybackStartTime() const
{
	return m_playbackInitSeek;
}

void UEvercoastStreamingReaderComp::SetPlaybackMicroTimeManagement(float overrideCurrTime, float overrideDeltaTime, float blockOnTimestamp)
{
	if (m_timestampDriver)
		m_timestampDriver->EnterSequencerTimestampOverride(overrideCurrTime, blockOnTimestamp);
}

void UEvercoastStreamingReaderComp::RemovePlaybackMicroTimeManagement()
{
	if (m_timestampDriver)
		m_timestampDriver->ExitSequencerTimestampOverride();

	// Act like the playback has been paused. Otherwise timestamp driver will keep driving the timestamp forward and
	// the playback continues unexpectedly
	StreamingPause();
}

TArray<FString> UEvercoastStreamingReaderComp::GetDataBitRates() const
{
	if (m_reader)
	{
		auto bitRates = m_reader->GetBitRates();
		bitRates.Sort([](uint32_t a, uint32_t b)
			{
				return a > b;
			});
		TArray<FString> result;
		result.Add("Unlimited");
		for(auto bitRate : bitRates)
		{
			// Megabits, ceil
			result.Add(FString::FromInt((bitRate >> 20) + 1));
		}
		return result;
	}
	else
	{
		return { "Unlimited", "300", "150", "60" };
	}
}


TArray<FString> UEvercoastStreamingReaderComp::GetAvailableFrameRates() const
{
	if (m_reader)
	{
		auto frameRates = m_reader->GetAvailableFrameRates();
		frameRates.Sort([](uint32_t a, uint32_t b)
			{
				return a > b;
			});
		TArray<FString> result;
		result.Add("Highest");
		for (auto frameRate : frameRates)
		{
			result.Add(FString::FromInt(frameRate));
		}
		return result;
	}
	else
	{
		return { "Highest", "30", "15" };
	}
}


bool UEvercoastStreamingReaderComp::HasReaderFatalError() const
{
	return m_readerHasFatalError;
}

FString UEvercoastStreamingReaderComp::GetReaderFatalError()
{
	return m_readerFataErrorMessage;
}
