// Fill out your copyright notice in the Description page of Project Settings.


#include "EvercoastStreamingReaderComp.h"
#include "EvercoastVoxelDecoder.h"
#include "CortoDecoder.h"
#include "WebpDecoder.h"
#include "Gaussian/EvercoastGaussianSplatDecoder.h"
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
	bPreferVideoCodec(false),
	bAutoPlay(true),
	bLoop(true),
	bSlackTiming(false),
	Renderer(nullptr),
	RendererActor(nullptr),
	m_baseDecoderType(DT_Invalid),
	m_playbackStatus(PlaybackStatus::Stopped),
	m_isReaderWaitingForData(true),
	m_isReaderInSeeking(false),
	m_isReaderPlaybackReady(false),
	m_isReaderWaitingForAudioData(true),
	m_videoTextureHog(nullptr),
	m_syncStatus(SyncStatus::InSync),
	m_lastMismatchedFrame(-1),
	m_consecutiveMismatchedFrameCount(0),
	m_playbackInitSeek(0),
	m_gtSeekStage(GTS_DEFAULT),
	m_vdSeekStage(VDS_DEFAULT),
	m_readerHasFatalError(false),
	m_currentMatchingFrameNumber(0),
	m_currentMatchingTimestamp(0.0f),
	m_lastDueTimestamp(0)
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

	m_baseDecoderType = DT_Invalid;
	m_reader = UGhostTreeFormatReader::Create(GetWorld()->WorldType == EWorldType::Editor, m_audioComponent, MaxCacheSizeInMB, this);
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
		//m_baseDecoder = nullptr;
		//m_auxDecoder = nullptr;

		// No file opened, but still need to keep promise
		m_fileOpenPromise = std::promise<void>();
		m_fileOpenFuture = m_fileOpenPromise.get_future();
		m_fileOpenPromise.set_value();
		return;
	}

	if (ECVAsset->GetDataURL().EndsWith(".ecv"))
	{
		//m_baseDecoder = EvercoastVoxelDecoder::Create();
		m_baseDecoderType = DecoderType::DT_EvercoastVoxel;
	}
	else if (ECVAsset->GetDataURL().EndsWith(".ecm"))
	{
		m_baseDecoderType = DecoderType::DT_CortoMesh;
		//m_baseDecoder = CortoDecoder::Create();
		//m_auxDecoder = WebpDecoder::Create();
	}
	else if (ECVAsset->GetDataURL().EndsWith("ecz"))
	{
		m_baseDecoderType = DecoderType::DT_EvercoastSpz;
		//m_baseDecoder = EvercoastGaussianSplatDecoder::Create();
	}
	else
	{
		m_dataDecoder = nullptr;
		//m_baseDecoder = nullptr;
		//m_auxDecoder = nullptr;
		m_baseDecoderType = DecoderType::DT_Invalid;

		UE_LOG(EvercoastReaderLog, Error, TEXT("Asset suffix is neither .ecv nor .ecm. Cannot choose a decoder."));
		OnFatalError("Cannot Choose Decoder Error");
		return;
	}

	m_dataDecoder = std::make_shared<EvercoastAsyncStreamingDataDecoder>(m_baseDecoderType);

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
	//m_baseDecoder = nullptr;
	//m_auxDecoder = nullptr;
	m_baseDecoderType = DecoderType::DT_Invalid;
	m_readerHasFatalError = false;
	m_currentMatchingFrameNumber = 0;
	m_currentMatchingTimestamp = 0.0f;
	m_audioComponent = nullptr;
	m_lastDueTimestamp = 0;

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
				runtimeAudio->SeekToTime(GetPlaybackTiming());

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
	if (m_videoTextureHog && m_baseDecoderType == DT_CortoMesh)
	{
		check(m_reader->MeshRequiresExternalData());
		if (play)
		{
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
			if (m_baseDecoderType != DecoderType::DT_Invalid)
				Renderer->ChooseCorrespondingSubRenderer(m_baseDecoderType);
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

	if (m_baseDecoderType == DT_CortoMesh && m_videoTextureHog)
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
		/*
		if (inSeeking)
		{
			m_timestampDriver->ResetTimerTo(m_reader->GetSeekingTarget(), m_playbackStatus == PlaybackStatus::Playing);
		}
		else
		{
			m_timestampDriver->ResetTimerTo(m_reader->GetCurrentTimestamp(), m_playbackStatus == PlaybackStatus::Playing);
		}
		*/
		m_timestampDriver->ResetTimerTo(m_reader->GetSeekingTarget(), m_playbackStatus == PlaybackStatus::Playing);
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
}


bool UEvercoastStreamingReaderComp::IsStreamingDurationReliable() const
{
	if (!m_reader)
		return false;

	if (m_baseDecoderType == DT_CortoMesh && m_videoTextureHog)
	{
		return m_videoTextureHog->IsVideoOpened();
	}

	return true;
}

void UEvercoastStreamingReaderComp::WaitForDurationBecomesReliable()
{
	if (m_baseDecoderType == DT_CortoMesh && m_videoTextureHog)
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
	return m_isReaderPlaybackReady && !m_isReaderInSeeking && !m_isReaderWaitingForData && !m_isReaderWaitingForAudioData;
}

void UEvercoastStreamingReaderComp::_PrintDebugStatus() const
{
	UE_LOG(EvercoastReaderLog, Verbose, TEXT("PlaybackReady: %d\tInSeeking: %d\tWaitingForData: %d\tWaitingForAudioData: %d"), 
		m_isReaderPlaybackReady, m_isReaderInSeeking, m_isReaderWaitingForData, m_isReaderWaitingForAudioData);

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
	m_timestampDriver = MakeShared<FTimestampDriver, ESPMode::ThreadSafe>(m_reader->GetDuration());
	
	if (m_baseDecoderType == DT_CortoMesh)
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

				FName name = MakeUniqueObjectName((UObject*)GetTransientPackage(), UFFmpegVideoTextureHog::StaticClass(), FName(this->GetName() + TEXT("_VideoTextureHog")));
				m_videoTextureHog = NewObject<UFFmpegVideoTextureHog>((UObject*)GetTransientPackage(), name);

				UE_LOG(EvercoastReaderLog, Log, TEXT("%s using ffmpeg decoder."), *UGameplayStatics::GetPlatformName());
			}

			//m_cortoTexSeekStage = CTS_DEFAULT;
			m_gtSeekStage = GTS_DEFAULT;
			m_vdSeekStage = VDS_DEFAULT;

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
			//m_cortoTexSeekStage = CTS_NA;
			m_gtSeekStage = GTS_DEFAULT;
			m_vdSeekStage = VDS_DEFAULT;
		}
	}
	else
	{
		//m_cortoTexSeekStage = CTS_NA;
		m_gtSeekStage = GTS_DEFAULT;
		m_vdSeekStage = VDS_DEFAULT;
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

	if (m_baseDecoderType == DT_CortoMesh ||
		m_baseDecoderType == DT_EvercoastVoxel ||
		m_baseDecoderType == DT_EvercoastSpz)
	{
		DoRefreshRenderer();

	}
	else
	{
		ensureMsgf(false, TEXT("Unknown decoder type: %d. Cannot create renderer."), (int)m_baseDecoderType);
		// keep promise
		m_fileOpenPromise.set_value();
		ret = false;
	}

	return ret;
}

void UEvercoastStreamingReaderComp::DoRefreshRenderer()
{
	if (m_baseDecoderType == DT_Invalid)
		return;

	if (Renderer)
		Renderer->ChooseCorrespondingSubRenderer(m_baseDecoderType);
}

bool UEvercoastStreamingReaderComp::OnCloseConnection(bool prerequisitySucceeded)
{
	if (m_baseDecoderType == DT_CortoMesh)
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


bool UEvercoastStreamingReaderComp::IsFrameCached(double testTimestamp) const
{
	if (m_dataDecoder)
	{
		if ((m_baseDecoderType == DT_EvercoastVoxel ||
			m_baseDecoderType == DT_EvercoastSpz ) && Renderer)
		{
			auto result = m_dataDecoder->QueryResult(testTimestamp);
			if (result && result->DecodeSuccessful)
			{
				return true;
			}
		}
		else if (m_baseDecoderType == DT_CortoMesh)
		{
			// MESH + WEBP TEX
			if (!m_reader->MeshRequiresExternalData())
			{
				auto result = m_dataDecoder->QueryResult(testTimestamp);
				if (result && result->DecodeSuccessful)
				{
					return true;
				}
			}
			else
			{
				auto result = m_dataDecoder->QueryResult(testTimestamp);
				if (result && result->DecodeSuccessful)
				{
					std::shared_ptr<CortoWebpUnifiedDecodeResult> unifiedResult = std::static_pointer_cast<CortoWebpUnifiedDecodeResult>(result);
					UTexture* decodedTexture = FindVideoTexture(unifiedResult->frameIndex);
					if (decodedTexture)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

void UEvercoastStreamingReaderComp::TickSequencerPlayback(float clipDuration)
{
	constexpr float EPOCH_TIME = 0.0f; // should rewind to 0, the very beginning

	bool isPlayingOrPause = (m_playbackStatus == PlaybackStatus::Playing) || (m_playbackStatus == PlaybackStatus::Paused);
	float dueTimestamp = GetPlaybackTiming();

	bool loopInProgress = m_gtSeekStage != GTS_DEFAULT || m_vdSeekStage != VDS_DEFAULT;

	// DEAL WITH LOOPING
	bool videoHitEOF = false;
	if (m_videoTextureHog && m_videoTextureHog->IsEndReached())
		videoHitEOF = true;

	bool rewindInTime = false;
	if (dueTimestamp < m_lastDueTimestamp)
	{
		// We need this check because we want to keep the 1s cache for a short rewinding in sequencer
		if (m_videoTextureHog && m_baseDecoderType== DT_CortoMesh)
		{
			auto meshResult = m_dataDecoder->QueryResult(dueTimestamp);
			if (meshResult == nullptr ||
				FindVideoTexture(meshResult->frameIndex) == nullptr)
			{
				// Can't find a cache, that means a big rewind, i.e. loop back to the beginning
				rewindInTime = true;
			}
		}
		else
		{
			if (m_dataDecoder->QueryResult(dueTimestamp) == nullptr)
			{
				rewindInTime = true;
			}
		}
		
	}
		
	
	if (clipDuration > 0 && ((dueTimestamp + m_reader->GetFrameInterval() >= clipDuration) || loopInProgress || videoHitEOF || rewindInTime))
	{
		
		dueTimestamp = EPOCH_TIME; // avoid false trimming

		if (m_videoTextureHog && m_baseDecoderType == DT_CortoMesh)
		{
				
			if (m_vdSeekStage == VDS_DEFAULT)
			{
				m_vdSeekStage = VDS_REQUESTED;

				m_videoTextureHog->ResetTo(EPOCH_TIME, [this, EPOCH_TIME]() {
					UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("VideoTextureHog Seek to %.2f completed"), EPOCH_TIME);
					this->m_vdSeekStage = VDS_COMPLETED;
					this->OnWaitingForVideoDataChanged(false); // force clean the wait for video flag

					this->m_syncStatus = SyncStatus::InSync;
					}
				);
			}

			if (m_gtSeekStage == GTS_DEFAULT)
			{
				m_gtSeekStage = GTS_REQUESTED;
				m_reader->RequestFrameOnTimestamp(EPOCH_TIME, [this, EPOCH_TIME]() {

					UE_LOG(EvercoastReaderLog, Log, TEXT("GT Seek to %.2f completed"), EPOCH_TIME);
					this->m_gtSeekStage = GTS_COMPLETED;

					});
			}


		}
		else
		{
			if (m_gtSeekStage == GTS_DEFAULT)
			{
				m_gtSeekStage = GTS_REQUESTED;
				// check curr timestamp too so avoid repeatedly request seeking(==infinite seeking and heavy IO)...
				m_reader->RequestFrameOnTimestamp(EPOCH_TIME, [this]() {
					this->m_gtSeekStage = GTS_COMPLETED;
					});
			}

		}


		
	}

	if (loopInProgress)
	{
		bool seekFinished = false;

		if (m_videoTextureHog && m_baseDecoderType == DT_CortoMesh)
		{
			if (m_vdSeekStage == VDS_COMPLETED && m_gtSeekStage == GTS_COMPLETED)
			{
				seekFinished = true;
			}
		}
		else
		{
			if (m_gtSeekStage == GTS_COMPLETED)
			{
				seekFinished = true;
			}
		}

		if (seekFinished)
		{
			// when the seeking is successful(almost)
			if (IsReaderPlayableWithoutBlocking())
			{
				// flush the decoder's remaining result. This needs to be done
				// for corto mesh/video decoder because the mismatch num of video frames
				// and geo frames and can cause delays
				m_syncStatus = SyncStatus::InSync; // reset sync status

				// force reset timer
				m_timestampDriver->ResetTimer();
				// re-get timestamp
				dueTimestamp = GetPlaybackTiming();

				ToggleAuxPlaybackIfPossible(true);

				m_vdSeekStage = VDS_DEFAULT;
				m_gtSeekStage = GTS_DEFAULT;

				loopInProgress = false;

			}
		}
	}

	m_lastDueTimestamp = dueTimestamp;

	if (loopInProgress)
		return;

	

	if (m_dataDecoder)
	{
		if ((m_baseDecoderType == DT_EvercoastVoxel || m_baseDecoderType == DT_EvercoastSpz) && Renderer)
		{
			std::vector<std::shared_ptr<IEvercoastStreamingDataUploader>> uploaders = Renderer->GetDataUploaders();
			if (!uploaders.empty())
			{
				auto result = m_dataDecoder->QueryResult(dueTimestamp);
				if (result && result->DecodeSuccessful)
				{
					// Uploader to check for voxel frame duplication
					for (auto uploader : uploaders)
					{
						uploader->Upload(result.get());
					}
					m_currentMatchingFrameNumber = result->frameIndex;
					m_currentMatchingTimestamp = result->frameTimestamp;
					m_syncStatus = SyncStatus::InSync;
				}
				else
				{
					m_syncStatus = SyncStatus::WaitForGT;
				}
			}
		}
		else if (m_baseDecoderType == DT_CortoMesh)
		{
			// MESH + WEBP TEX
			if (!m_reader->MeshRequiresExternalData())
			{
				auto result = m_dataDecoder->QueryResult(dueTimestamp);
				if (result && result->DecodeSuccessful)
				{
					check(result->GetType() == DRT_CortoMesh_WebpImage_Unified);

					std::vector<std::shared_ptr<IEvercoastStreamingDataUploader>> uploaders = Renderer->GetDataUploaders();
					if (!uploaders.empty())
					{
						std::shared_ptr<CortoWebpUnifiedDecodeResult> unifiedResult = std::static_pointer_cast<CortoWebpUnifiedDecodeResult>(result);

						auto meshResult = unifiedResult->meshResult;
						auto imgResult = unifiedResult->imgResult;
						check(meshResult->DecodeSuccessful && imgResult->DecodeSuccessful && meshResult->frameIndex == imgResult->frameIndex);
						check(meshResult->GetType() == DRT_CortoMesh && imgResult->GetType() == DRT_WebpImage);

						// Uploader to check for mesh frame duplication
						for (auto uploader : uploaders)
						{
							uploader->Upload(unifiedResult.get());
						}

						m_currentMatchingFrameNumber = unifiedResult->frameIndex;
						m_currentMatchingTimestamp = unifiedResult->frameTimestamp;
						m_syncStatus = SyncStatus::InSync;
					}
				}
				else
				{
					m_syncStatus = SyncStatus::WaitForGT;
				}
			}
			// MESH + VIDEO TEX
			else
			{
				auto result = m_dataDecoder->QueryResult(dueTimestamp);
				if (result && result->DecodeSuccessful)
				{
					check(result->GetType() == DRT_CortoMesh_WebpImage_Unified);

					std::vector<std::shared_ptr<IEvercoastStreamingDataUploader>> uploaders = Renderer->GetDataUploaders();
					if (!uploaders.empty())
					{
						std::shared_ptr<CortoWebpUnifiedDecodeResult> unifiedResult = std::static_pointer_cast<CortoWebpUnifiedDecodeResult>(result);
						UTexture* decodedTexture = FindVideoTexture(unifiedResult->frameIndex);
						if (decodedTexture)
						{
							// HACKHACK: hitchhike the payload
							unifiedResult->videoTextureResult = decodedTexture;

							// Uploader to check for mesh frame duplication
							for (auto uploader : uploaders)
							{
								uploader->Upload(unifiedResult.get());
							}

							m_currentMatchingFrameNumber = unifiedResult->frameIndex;
							m_currentMatchingTimestamp = unifiedResult->frameTimestamp;
							m_syncStatus = SyncStatus::InSync;

							UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Geom/Tex result matched: %.2f Coerced To: %.2f "), dueTimestamp, m_currentMatchingTimestamp);
						}
						else
						{
							m_syncStatus = SyncStatus::WaitForVideo;

							UE_LOG(EvercoastReaderLog, Log, TEXT("Tex result not found: %.2f"), dueTimestamp);
						}


					}
				}
				else
				{
					UE_LOG(EvercoastReaderLog, Log, TEXT("Geom result not found: %.2f"), dueTimestamp);

					m_syncStatus = SyncStatus::WaitForGT;
				}

				// To move video decoder's cache forward
				TrimVideoCache(dueTimestamp);
			}
		}
		else
		{
			// Do nothing, no data should be held in any base decoder
		}

		// To move data decoder's cache forward
		if (m_dataDecoder->TrimCache(dueTimestamp))
		{
			// after trimming, we'll need to call to request reading API's next frame (RequestFrameNext)
			m_reader->ContinueRequest();
		}

	}
}

void UEvercoastStreamingReaderComp::TickNormalPlayback(float clipDuration)
{
	constexpr float EPOCH_TIME = 0.0f; // should rewind to 0, the very beginning

	bool isPlayingOrPause = (m_playbackStatus == PlaybackStatus::Playing) || (m_playbackStatus == PlaybackStatus::Paused);

	float dueTimestamp = GetPlaybackTiming();
	bool scrutinizeTime = false;
	bool isPlaying = m_playbackStatus == PlaybackStatus::Playing;

	if (m_audioComponent && m_audioComponent->Sound && m_audioComponent->Sound->IsA<URuntimeAudio>())
	{
		scrutinizeTime = true;
	}

	if (!isPlaying)
		scrutinizeTime = false; //  no need to scrutinize time when paused

	// Override scrutinizeTime
	if (bSlackTiming)
		scrutinizeTime = false;

	bool loopInProgress = m_gtSeekStage != GTS_DEFAULT || m_vdSeekStage != VDS_DEFAULT;
	//////////////////////////////////////////////////////////////
	// Delta time calculation, micro time management and looping
	if (m_dataDecoder
		&& isPlayingOrPause
		&& !m_reader->IsInSeeking()
		)
	{
		// DEAL WITH DURATION INCONSISTENCY BETWEEN GT AND VIDEO
		float duration = clipDuration;
		UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Due time: %.3f, playback timer: %.3f, duration: %.3f"), dueTimestamp, GetPlaybackTiming(), duration);

		// DEAL WITH LOOPING
		bool videoHitEOF = false;
		if (m_videoTextureHog && m_videoTextureHog->IsEndReached())
			videoHitEOF = true;

		if (bLoop && duration > 0 && ((dueTimestamp + m_reader->GetFrameInterval() >= duration) || loopInProgress || videoHitEOF))
		{
			bool seekFinished = false;

			if (m_videoTextureHog && m_baseDecoderType == DT_CortoMesh)
			{
				if (m_vdSeekStage == VDS_DEFAULT)
				{
					m_vdSeekStage = VDS_REQUESTED;

					m_videoTextureHog->ResetTo(EPOCH_TIME, [this, EPOCH_TIME]() {
						UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("VideoTextureHog Seek to %.2f completed"), EPOCH_TIME);
						this->m_vdSeekStage = VDS_COMPLETED;
						this->OnWaitingForVideoDataChanged(false); // force clean the wait for video flag

						this->m_syncStatus = SyncStatus::InSync;
						}
					);
				}

				if (m_gtSeekStage == GTS_DEFAULT)
				{
					m_gtSeekStage = GTS_REQUESTED;
					m_reader->RequestFrameOnTimestamp(EPOCH_TIME, [this, EPOCH_TIME]() {

						UE_LOG(EvercoastReaderLog, Log, TEXT("GT Seek to %.2f completed"), EPOCH_TIME);
						this->m_gtSeekStage = GTS_COMPLETED;

						});
				}

				if (m_vdSeekStage == VDS_COMPLETED && m_gtSeekStage == GTS_COMPLETED)
				{
					seekFinished = true;
				}
			}
			else
			{
				if (m_gtSeekStage == GTS_DEFAULT)
				{
					m_gtSeekStage = GTS_REQUESTED;
					// check curr timestamp too so avoid repeatedly request seeking(==infinite seeking and heavy IO)...
					m_reader->RequestFrameOnTimestamp(EPOCH_TIME, [this]() {
						this->m_gtSeekStage = GTS_COMPLETED;
						});
				}

				if (m_gtSeekStage == GTS_COMPLETED)
				{
					seekFinished = true;
				}
			}

			

			if (seekFinished)
			{
				// when the seeking is successful(almost)
				if (IsReaderPlayableWithoutBlocking())
				{
					m_syncStatus = SyncStatus::InSync; // reset sync status

					// force reset timer
					m_timestampDriver->ResetTimer();
					// re-get timestamp
					dueTimestamp = GetPlaybackTiming();

					ToggleAuxPlaybackIfPossible(true);

					m_vdSeekStage = VDS_DEFAULT;
					m_gtSeekStage = GTS_DEFAULT;
				}
			}

			// Update loopInProgress
			loopInProgress = m_gtSeekStage != GTS_DEFAULT || m_vdSeekStage != VDS_DEFAULT;

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

	if (loopInProgress)
		return;

	/////////////////////////////////////////
	// Data uploading
	if (m_dataDecoder)
	{
		float trimmingMedian = -1;

		// VOXEL & Splats, QUERY AND UPLOAD, STRAIGHTFORWARD 
		if (m_baseDecoderType == DT_EvercoastVoxel ||
			m_baseDecoderType == DT_EvercoastSpz )
		{
			if (Renderer)
			{
				std::vector<std::shared_ptr<IEvercoastStreamingDataUploader>> uploaders = Renderer->GetDataUploaders();
				float lastQueriedTimestamp = m_currentMatchingTimestamp;
				auto result = m_dataDecoder->QueryResult(dueTimestamp);

				if (!uploaders.empty() && result && result->DecodeSuccessful)
				{
					if (!scrutinizeTime)
					{
						auto nextFrameResult = m_dataDecoder->QueryResultAfterTimestamp(lastQueriedTimestamp);

						// Check here if we have a more "adjacent" frame to upload
						if (nextFrameResult && nextFrameResult->DecodeSuccessful && nextFrameResult->frameTimestamp < result->frameTimestamp)
						{
							// use next frame, as we don't want to skip frames
							result = nextFrameResult;
							trimmingMedian = nextFrameResult->frameTimestamp;
						}
					}

					if (result && result->DecodeSuccessful)
					{
						if (m_syncStatus == SyncStatus::WaitForGT)
						{
							// Come back from pause-to-cache
							m_syncStatus = SyncStatus::InSync;

							OnWaitingForDataChanged(false);
						}

						// Uploader should check for frame duplication
						for (auto uploader : uploaders)
						{
							uploader->Upload(result.get());
						}
						m_currentMatchingFrameNumber = result->frameIndex;
						m_currentMatchingTimestamp = result->frameTimestamp;
					}
					else
					{
						// Wait
						if (m_dataDecoder->IsTimestampBeyondCache(dueTimestamp))
						{
							OnWaitingForDataChanged(true);

							m_syncStatus = SyncStatus::WaitForGT;
						}
					}
				}
				else
				{
					// Wait
					if (m_dataDecoder->IsTimestampBeyondCache(dueTimestamp))
					{
						OnWaitingForDataChanged(true);

						m_syncStatus = SyncStatus::WaitForGT;
					}
				}
			}
		}
		// MESH + VIDEO OR MESH + WEBP
		else if (m_baseDecoderType == DT_CortoMesh)
		{
			if (Renderer)
			{
				std::vector<std::shared_ptr<IEvercoastStreamingDataUploader>> uploaders = Renderer->GetDataUploaders();
				auto result = m_dataDecoder->QueryResult(dueTimestamp);

				if (!uploaders.empty() && result && result->DecodeSuccessful)
				{
					float lastQueriedTimestamp = m_currentMatchingTimestamp;

					// MESH + VIDEO TEXTURE IS COMPLEX
					// When external video data is required
					if (m_reader->MeshRequiresExternalData())
					{
						if (!scrutinizeTime)
						{
							auto nextFrameResult = m_dataDecoder->QueryResultAfterTimestamp(lastQueriedTimestamp);

							// Check here if we have a more "adjacent" frame to upload
							if (nextFrameResult && nextFrameResult->DecodeSuccessful && nextFrameResult->frameTimestamp < result->frameTimestamp)
							{
								// use next frame, as we don't want to skip frames
								result = nextFrameResult;
								trimmingMedian = nextFrameResult->frameTimestamp;
							}
						}

						// this should be true most of the time, but it can be false when threaded decoding
						// changes the internal status of the decoder, during the two peeks. Chances are slim tho
						if (result && result->DecodeSuccessful)
						{
							

							if (m_syncStatus == SyncStatus::WaitForGT)
							{
								// Come back from pause-to-cache
								m_syncStatus = SyncStatus::InSync;

								OnWaitingForDataChanged(false);
							}

							check(result->GetType() == DRT_CortoMesh_WebpImage_Unified);
							auto pResult = std::static_pointer_cast<CortoWebpUnifiedDecodeResult>(result);

							UTexture* decodedTexture = FindVideoTexture(pResult->frameIndex);
							// EVERYTHING GOES NORMAL
							if (decodedTexture)
							{
								UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Decoded uploaded at frame: %d, timestamp: %.3f, dueTime: %.3f"), pResult->frameIndex, pResult->frameTimestamp, dueTimestamp);


								// hitchhike the payload
								pResult->videoTextureResult = decodedTexture;
								// Uploader and decoder will have to properly handle the lock/unlock of the decoded result,
								// as the results are all preallocated and cached and reusable.
								for (auto uploader : uploaders)
								{
									uploader->Upload(pResult.get());
								}

								if (m_syncStatus == SyncStatus::WaitForVideo)
								{
									// Come back from pause-to-cache
									m_syncStatus = SyncStatus::InSync;

									OnWaitingForDataChanged(false);
								}

								m_currentMatchingFrameNumber = pResult->frameIndex;
								m_currentMatchingTimestamp = pResult->frameTimestamp;

							}
							// VIDEO FRAME MISSING, STOP THE TIMER AND MARK WAIT
							else
							{

								if (IsTimestampBeyondVideoCache(dueTimestamp))
								{
									m_syncStatus = SyncStatus::WaitForVideo;
									OnWaitingForDataChanged(true);
								}
								else
								{
									// Do nothing just let timer run
								}

								
							}
						}

						// Trim video cache
						float videoMedianTimestamp = dueTimestamp;
						if (trimmingMedian >= 0)
						{
							videoMedianTimestamp = trimmingMedian;
						}
						TrimVideoCache(videoMedianTimestamp);
					}
					else
					{
						// MESH + WEBP TEXTURE, QUERY AND UPLOAD
						check(result->GetType() == DRT_CortoMesh_WebpImage_Unified)

						if (!scrutinizeTime)
						{
							auto nextFrameResult = m_dataDecoder->QueryResultAfterTimestamp(lastQueriedTimestamp);

							// Check here if we have a more "adjacent" frame to upload
							if (nextFrameResult && nextFrameResult->DecodeSuccessful && nextFrameResult->frameTimestamp < result->frameTimestamp)
							{
								// use next frame, as we don't want to skip frames
								result = nextFrameResult;
								trimmingMedian = nextFrameResult->frameTimestamp;
							}
						}

						if (result && result->DecodeSuccessful)
						{
							if (m_syncStatus == SyncStatus::WaitForGT)
							{
								// Come back from pause-to-cache
								m_syncStatus = SyncStatus::InSync;

								OnWaitingForDataChanged(false);
							}

							std::shared_ptr<CortoWebpUnifiedDecodeResult> unifiedResult = std::static_pointer_cast<CortoWebpUnifiedDecodeResult>(result);

							auto meshResult = unifiedResult->meshResult;
							auto imgResult = unifiedResult->imgResult;
							check(meshResult->DecodeSuccessful && imgResult->DecodeSuccessful && meshResult->frameIndex == imgResult->frameIndex);
							check(meshResult->GetType() == DRT_CortoMesh && imgResult->GetType() == DRT_WebpImage);

							for (auto uploader : uploaders)
							{
								uploader->Upload(unifiedResult.get());
							}

							m_currentMatchingFrameNumber = unifiedResult->frameIndex;
							m_currentMatchingTimestamp = unifiedResult->frameTimestamp;
						}
						else
						{
							// Wait
							if (m_dataDecoder->IsTimestampBeyondCache(dueTimestamp))
							{
								OnWaitingForDataChanged(true);

								m_syncStatus = SyncStatus::WaitForGT;
							}
						}

					}
				}
				else
				{
					// if we cannot get a result here, which is very likely in http streaming, we'll need to 
					// 1. tell why we cannot get a result, is the expected result still not cached?
					// 2. if yes, then we'll notify the status callback OnWaitingForDataChanged() and make waiting marks here
					// 3. if no, something strange and shouldn't happen, but proceed with
					// 4. once the data is cached, and the waiting mark is detected, OnWaitingForDataChanged() should be called again

					if (m_dataDecoder->IsTimestampBeyondCache(dueTimestamp))
					{
						OnWaitingForDataChanged(true);

						m_syncStatus = SyncStatus::WaitForGT;
					}
					else
					{
						// Otherwise this will be strange situation, where the data is already trimmed(left behind), but still this timestamp 
						// will require it, which could be a trimming algorithm bug or concurrent issue. Just let the time run and hopefully
						// cache up later.

						// FIXME: some corto+mesh asset will have corto blocks received consecutively for several seconds, rather than 
						// interlaced with corto -> webp -> corto -> webp order this will cause disruption.
						// MAYBE TRY FORCE CHANGE IT BACK TO SYNC
					}
				}
			}
		}
		else
		{
			// just dispose whatever was decoded
			m_dataDecoder->FlushAndDisposeResults();
		}

		// To move data decoder's cache forward
		float medianTimestamp = dueTimestamp;
		if (trimmingMedian >= 0)
		{
			medianTimestamp = trimmingMedian;
		}

		if (m_dataDecoder->TrimCache(medianTimestamp))
		{
			// after trimming, we'll need to call to request reading API's next frame (RequestFrameNext)
			m_reader->ContinueRequest();
		}
	}
}

// Called every frame
void UEvercoastStreamingReaderComp::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// DEAL WITH DURATION INCONSISTENCY BETWEEN GT AND VIDEO
	if (m_baseDecoderType != DT_Invalid)
	{
		float duration;
		if (m_videoTextureHog && m_videoTextureHog->IsVideoOpened() && m_baseDecoderType == DT_CortoMesh)
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

		bool isInSequencer = false;
		if (m_timestampDriver && m_timestampDriver->IsSequencerOverriding())
		{
			isInSequencer = true;
		}

		if (isInSequencer)
		{
			TickSequencerPlayback(duration);
		}
		else
		{
			TickNormalPlayback(duration);
		}
	}

	


	/////////////////////////////////////////
	// Misc components tick
	if (m_reader)
		m_reader->Tick();

	if (m_videoTextureHog)
		m_videoTextureHog->Tick(GetWorld());
	/////////////////////////////////////////
}

UTexture* UEvercoastStreamingReaderComp::FindVideoTexture(int64_t frameIndex) const
{
	check(m_videoTextureHog);
	// find video texture from video texture hog
	return m_videoTextureHog->QueryTextureAtIndex(frameIndex);
}

void UEvercoastStreamingReaderComp::TrimVideoCache(double medianTimestamp)
{
	check(m_videoTextureHog);
	m_videoTextureHog->TrimCache(medianTimestamp, 0.5 * m_reader->GetFrameInterval());
}

bool UEvercoastStreamingReaderComp::IsTimestampBeyondVideoCache(double timestamp) const
{
	check(m_videoTextureHog);
	return m_videoTextureHog->IsTextureBeyondRange(timestamp, 0.5 * m_reader->GetFrameInterval());
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

	if (m_playbackStatus == PlaybackStatus::Paused ||
		m_playbackStatus == PlaybackStatus::Playing)
	{
		// if cache contains the frame, we just turn the clock
		// otherwise, we'll need a full dispose-seek-cache circle
		if (IsFrameCached(timestamp))
		{
			m_timestampDriver->ResetTimerTo(timestamp, false);
		}
		else
		{
			m_reader->RequestFrameOnTimestamp(timestamp);

			// Request video texture hogging too (we don't have other means to sync the video hogger, and ToggleVideoTextureHogIfPossible() is not for this purpose)
			if (m_videoTextureHog && m_baseDecoderType == DT_CortoMesh)
			{
				check(m_reader->MeshRequiresExternalData());
				m_videoTextureHog->ResetTo(timestamp);
				if (!m_videoTextureHog->IsHogging())
					m_videoTextureHog->StartHogging();
			}
		}
		
	}
}

void UEvercoastStreamingReaderComp::StreamingJump(float deltaTime)
{
	if (!m_reader)
		return;

	float timestamp = m_currentMatchingTimestamp + deltaTime;
	timestamp = std::min(std::max(timestamp, 0.0f), StreamingGetDuration());

	StreamingSeekTo(timestamp);
}

void UEvercoastStreamingReaderComp::StreamingPrevFrame()
{
	if (!m_reader)
		return;

	float nextFrameTimestamp = std::max(m_currentMatchingTimestamp - m_reader->GetFrameInterval(), 0.0f);
	StreamingSeekTo(nextFrameTimestamp);
}

void UEvercoastStreamingReaderComp::StreamingNextFrame()
{
	if (!m_reader)
		return;

	float duration = StreamingGetDuration();
	float nextFrameTimestamp = std::min(m_currentMatchingTimestamp + m_reader->GetFrameInterval(), duration);

	StreamingSeekTo(nextFrameTimestamp);
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
	if (m_baseDecoderType == DT_CortoMesh && m_videoTextureHog)
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

int32 UEvercoastStreamingReaderComp::StreamingGetCurrentFrameRate() const
{
	if (!m_reader)
		return 0;

	return (int32)m_reader->GetFrameRate();
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

bool UEvercoastStreamingReaderComp::IsPlaybackInMicroTimeManagement() const
{
	if (m_timestampDriver)
	{
		return m_timestampDriver->IsSequencerOverriding();
	}

	return false;

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
