#include "Realtime/PicoQuicStreamingReaderComp.h"
#include "GhostTreeFormatReader.h"
#include "EvercoastDecoder.h"
#include "EvercoastVoxelRendererComp.h"
#include "EvercoastStreamingDataUploader.h"
#include "EvercoastRendererSelectorComp.h"

#include "Realtime/RealtimeNetworkThread.h"
#include "Realtime/EvercoastRealtimeStreamingVoxelDecoder.h"
#include "Realtime/EvercoastRealtimeStreamingCortoDecoder.h"

#include "Components/AudioComponent.h"
#include "Realtime/PicoAudioSoundWave.h"
#include "Math/UnrealMathUtility.h"

#include "Templates/SharedPointer.h"

#include "Realtime/EvercoastRealtimeConfig.h"

using namespace PicoQuic;

DEFINE_LOG_CATEGORY(EvercoastRealtimeLog);


#define SKIP_GEOM_FRAMES (0)


UPicoQuicStreamingReaderComp::UPicoQuicStreamingReaderComp(const FObjectInitializer& initializer) :
Super(initializer),
ServerPort(6655),
AutoConnect(true),
DebugLogging(false),
IgnoreAudio(false),
m_audioComponent(nullptr),
m_sound(nullptr),
m_SampledFrameNumStart(0),
m_SampledFrameNumEnd(0),
m_decoderType(DT_Invalid),
m_dataUploadingPaused(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;

	m_dataReceivingCounter = std::make_shared<EvercoastPerfCounter>("Data Reciving Rate", 2.0f);
	m_dataDecodingCounter = std::make_shared<EvercoastPerfCounter>("Data Decoding Rate", 2.0f);
	m_audioFrameMissCounter = std::make_shared<EvercoastPerfCounter>("Audio Frame Missing", 2.0f);
	m_videoLaggingCounter = std::make_shared<EvercoastPerfCounter>("Video Lagging After Compensation", 2.0f);
	m_videoBehindAudioCounter = std::make_shared<EvercoastPerfCounter>("Video Behind Audio", 2.0f);
	m_videoDiscardCounter = std::make_shared<EvercoastPerfCounter>("Video Discarded", 2.0f);

	
}

UPicoQuicStreamingReaderComp::~UPicoQuicStreamingReaderComp() = default;

void UPicoQuicStreamingReaderComp::ConnectExpress()
{
	Connect(ServerAddress, ServerPort, AuthToken);
}

static FString DefaultCertificatePath = TEXT("EvercoastPlayback/Content/Crypto/realtime.rootca.crt");

static FString CreateTempCertFile(const FString& certPath)
{
	// check if absolute path
	FString realtimeCryptoFilename;
	if (FPaths::IsRelative(certPath))
	{
		realtimeCryptoFilename = FPaths::Combine(FPaths::GetPath(FPaths::ProjectPluginsDir()), certPath);
	}
	else
	{
		realtimeCryptoFilename = certPath;
	}

	 

	IFileManager& fileManager = IFileManager::Get();
	FString tempDir = FPaths::Combine(FPlatformProcess::UserTempDir(), TEXT("EvercoastTemp"));
	// Clean the temp dir if still exists from previous run
	fileManager.DeleteDirectory(*tempDir, false, true);

	if (fileManager.MakeDirectory(*tempDir, true))
	{
		FString tempFilename = FPaths::CreateTempFilename(*tempDir, TEXT("Evercoast"), TEXT(".crt"));
		if (fileManager.Copy(*tempFilename, *realtimeCryptoFilename) != COPY_OK)
		{
			UE_LOG(EvercoastRealtimeLog, Error, TEXT("Failed to create temporary file: %s -> %s"), *realtimeCryptoFilename, *tempFilename);
			return FString();
		}

		return tempFilename;

	}

	return FString();
}

static void DeleteTempCertFile(const FString& filename)
{
	IFileManager& fileManager = IFileManager::Get();
	fileManager.Delete(*filename);

	FString tempDir = FPaths::GetPath(filename);
	fileManager.DeleteDirectory(*tempDir, false, true);
}

static void DeleteAllTempCertFiles()
{
	IFileManager& fileManager = IFileManager::Get();
	FString tempDir = FPaths::Combine(FPlatformProcess::UserTempDir(), TEXT("EvercoastTemp"));
	// Clean the temp dir 
	fileManager.DeleteDirectory(*tempDir, false, true);
}

void UPicoQuicStreamingReaderComp::Connect(const FString& serverName, int port, const FString& accessToken)
{
	Disconnect();

	m_audioComponent = GetOwner()->FindComponentByClass<UAudioComponent>();
	if (m_audioComponent)
	{
		m_audioComponent->bAlwaysPlay = true;
		m_audioComponent->bIsUISound = true;

		if (!IgnoreAudio)
		{
			m_sound = static_cast<UPicoAudioSoundWave*>(m_audioComponent->Sound);
			if (m_sound)
			{
				m_sound->ResetAudio();
			}
			else
			{
				m_sound = NewObject<UPicoAudioSoundWave>(GetTransientPackage(), UPicoAudioSoundWave::StaticClass());
				m_audioComponent->SetSound(m_sound);
			}

			m_sound->SetMissingFrameCounter(m_audioFrameMissCounter);
		}
		else
		{
			m_sound = nullptr;
		}
	}
	else
	{
		m_sound = nullptr;
	}

	if (m_dataDecoder)
	{
		for (auto it = m_decodedResultQueue.begin(); it != m_decodedResultQueue.end(); ++it)
		{
			m_dataDecoder->DisposeResult(*it);
		}

		m_decodedResultQueue.clear();
	}

	// Get config
	UEvercoastRealtimeConfig* config = NewObject<UEvercoastRealtimeConfig>();
	// Setup audio's warm up time
	if (m_sound)
	{
		m_sound->SetWarmupTime(config->WarmUpTime);
	}

	FString realtimeCryptoFilename = config->CertificationPath;

	FString authToken = accessToken;
	if (authToken.IsEmpty())
	{
		authToken = config->AccessToken;
	}

	FString certPath = CertificationPath;
	if (certPath.IsEmpty())
	{
		certPath = config->CertificationPath;
		if (certPath.IsEmpty())
		{
			// default path
			certPath = FString(DefaultCertificatePath);
		}
	}
	


	// Prepare temp certificate file
	FString tempFilename = CreateTempCertFile(certPath);

	m_networkThread = std::make_shared<RealtimeNetworkThread>();
	m_networkThread->Connect(TCHAR_TO_ANSI(*serverName), port, TCHAR_TO_ANSI(*authToken), TCHAR_TO_ANSI(*tempFilename), m_sound, 
		[this, tempFilename](uint32_t stream_type) {
			// NOTE: Cannot remove temp cert file here as we need to reconnect when lost connection

			m_onConnectionSuccessCallback = [this]() {
				if (OnConnectionSuccess.IsBound())
				{
					OnConnectionSuccess.Broadcast();
				}
			};

			return CreateDecoder(stream_type);
		}, 

		[this, tempFilename]() {
			// Remove temp cert file
			DeleteTempCertFile(tempFilename);

			m_onConnectionFailureCallback = [this](FString reason) {
				if (OnConnectionFailure.IsBound())
				{
					OnConnectionFailure.Broadcast(reason);
				}
			};
		},
		
		m_dataReceivingCounter);

	m_dataUploadingPaused = false;

	if (DebugLogging)
	{
		auto Owner = GetOwner();
		Owner->GetWorldTimerManager().SetTimer(m_StreamTimer, this, &UPicoQuicStreamingReaderComp::OnStreamTimer, 5.0f, false);
	}

}

void UPicoQuicStreamingReaderComp::Disconnect()
{
	DeleteAllTempCertFiles();

	if (m_networkThread)
	{
		m_networkThread->Disconnect();
		m_networkThread.reset();
	}
    
    if (m_audioComponent && m_audioComponent->Sound)
    {
        m_audioComponent->Stop();
        m_audioComponent->SetSound(nullptr);
    }
    
	if (Renderer)
	{
		Renderer->ResetRendererSelection();
	}

	if (m_dataDecoder)
	{
		for (auto it = m_decodedResultQueue.begin(); it != m_decodedResultQueue.end(); ++it)
		{
			m_dataDecoder->DisposeResult(*it);
		}

		m_decodedResultQueue.clear();
	}
	
    DestroyDecoder();
}

bool UPicoQuicStreamingReaderComp::IsConnected() const
{
	return m_networkThread != nullptr;
}

EPicoQuicStatus UPicoQuicStreamingReaderComp::GetStatus() const
{
	if (m_networkThread != nullptr)
	{
		return (EPicoQuicStatus)m_networkThread->GetStatus();
	}
	return EPicoQuicStatus::NotYetConnected;
}

bool UPicoQuicStreamingReaderComp::IsStatusGood() const
{
	switch (GetStatus()) {
		case EPicoQuicStatus::NotYetConnected:
		case EPicoQuicStatus::Connected:
			return true;
		default:
			return false;
	}
}


void UPicoQuicStreamingReaderComp::OnRegister()
{
	Super::OnRegister();
}

void UPicoQuicStreamingReaderComp::OnUnregister()
{

	if (IsConnected())
	{
		Disconnect();
	}

	Super::OnUnregister();
}

void UPicoQuicStreamingReaderComp::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (DebugLogging)
	{
		auto Owner = GetOwner();
		float CurrentElapsed = Owner->GetWorldTimerManager().GetTimerElapsed(m_StreamTimer);
		//	UE_LOG(EvercoastRealtimeLog, Verbose, TEXT("Elapsed time: %.2f"), CurrentElapsed);
		if (CurrentElapsed > 1.0f)
		{
			int64_t receivedFrames = 0;
			if (m_SampledFrameNumEnd > 0 && m_SampledFrameNumStart > 0)
				receivedFrames = m_SampledFrameNumEnd - m_SampledFrameNumStart + 1;

			UE_LOG(EvercoastRealtimeLog, Log, TEXT("Averaged framerate for last 1 sec: %.2f"), receivedFrames / CurrentElapsed);
			m_SampledFrameNumStart = 0;
			m_SampledFrameNumEnd = 0;

			Owner->GetWorldTimerManager().SetTimer(m_StreamTimer, this, &UPicoQuicStreamingReaderComp::OnStreamTimer, 5.0f, false);
		}
	}

	// Blueprint callbacks in main thread
	if (m_onConnectionSuccessCallback)
	{
		m_onConnectionSuccessCallback();
		m_onConnectionSuccessCallback = nullptr;
	}

	if (m_onConnectionFailureCallback)
	{
		m_onConnectionFailureCallback(TEXT("Authentication failure. Check access token and certificate."));
		m_onConnectionFailureCallback = nullptr;
	}

	//////////////////////////////////////////
	// Deal with visibility when stopped
	bool needRendering = true;
	if (!IsConnected() || !IsStatusGood())
	{
		if (Renderer && !Renderer->bKeepRenderedFrameWhenStopped)
		{
			needRendering = false;
		}
	}

	if (Renderer && needRendering != Renderer->IsVisible())
	{
		if (needRendering && m_dataDecoder != nullptr) // if need to set visibility to true, at least we need one new frame
		{
			Renderer->SetVisibility(true, true);
		}
		else // otherwise just invisible to avoid showing last rendered frame
		{
			Renderer->SetVisibility(false, true);
		}
	}
	//////////////////////////////////////////

	if (m_dataDecoder && Renderer)
	{
		auto uploader = Renderer->GetDataUploader();
		if (!uploader)
		{
			// We have to create sub renderer in main thread
			PrepareRenderer();
			uploader = Renderer->GetDataUploader();
		}

		if (uploader)
		{
			double audioTimestamp = 0;

			if (m_sound)
			{
				audioTimestamp = m_sound->GetLastFedPCMTimestamp();
			}

			auto result = m_dataDecoder->PopResult();
			bool needFurtherUploading = true;
			if (result)
			{

				if (m_sound)
				{
					double actualReceivedAudioVideoDiff = m_sound->GetLastReceivedPCMTimestamp() - result->frameTimestamp;
					m_videoBehindAudioCounter->AddSampleAsDouble(actualReceivedAudioVideoDiff);
				}

				// If there's sound and sound packets are arriving in timely order
				// we'll use audio data as a timestamp provider to guide
				// audio/video sync
				// that should be the most precise audio timestamp we can get
				if (m_sound && !IgnoreAudio)
				{
					m_sound->Sync(result->frameTimestamp);
					m_sound->Tick(DeltaTime);

					// Add delay to the sound buffer to match up sync-ing
					m_sound->SetAudioBufferDelay(m_videoLaggingCounter->GetSampleAverageDoubleOnCount());

					m_decodedResultQueue.push_back(result);
				}
				// otherwise we'll upload geometry data ASAP
				else
				{
					if (!m_dataUploadingPaused)
					{
						uploader->Upload(result.get());
						// manually release the result from decoder
						m_dataDecoder->DisposeResult(result);

						needFurtherUploading = false;
					}
				}
				

				if (!m_dataUploadingPaused && m_audioComponent && !m_audioComponent->IsPlaying())
				{
					m_audioComponent->Play();
				}
			}
			else
			{
				if (m_sound && !IgnoreAudio)
				{
					// Do extrapolation 
					m_sound->Tick(DeltaTime);
				}
			}

			// This pop operation needs to be done faster than receiving frames, otherwise the frame will pile up and won't reduce the geom/audio latency
			if (!m_dataUploadingPaused && m_sound && !IgnoreAudio && needFurtherUploading)
			{
				auto syncResult = PopSynchronisedResultRegardsAudioTimestamp(audioTimestamp, m_dataDecoder);
				if (syncResult)
				{
					uploader->Upload(syncResult.get());

					m_dataDecoder->DisposeResult(syncResult);

				}
			}
		}
	}
}

void UPicoQuicStreamingReaderComp::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(EvercoastRealtimeLog, Log, TEXT("Begin Play"));

	if (AutoConnect && !ServerAddress.IsEmpty() && ServerPort != 0)
	{
		ConnectExpress();
	}
}

void UPicoQuicStreamingReaderComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(EvercoastRealtimeLog, Log, TEXT("End Play"));
	Disconnect();

	Super::EndPlay(EndPlayReason);
}

void UPicoQuicStreamingReaderComp::OnStreamTimer()
{
	UE_LOG(EvercoastRealtimeLog, Log, TEXT("Stream timer tick"));
}

std::shared_ptr<IEvercoastStreamingDataDecoder> UPicoQuicStreamingReaderComp::CreateDecoder(uint32_t stream_type)
{
	if (stream_type == 0x304D4345) // "ECM0"
	{
		m_dataDecoder = std::make_shared<EvercoastRealtimeStreamingCortoDecoder>(m_dataDecodingCounter);
		m_decoderType = DT_CortoMesh;
	}
	else if (stream_type == 0x30564345) // "ECV0"
	{
		m_dataDecoder = std::make_shared<EvercoastRealtimeStreamingVoxelDecoder>(m_dataDecodingCounter);
		m_decoderType = DT_EvercoastVoxel;
	}
	else
	{
		UE_LOG(EvercoastRealtimeLog, Error, TEXT("Unknown decoder type: %d"), stream_type);
		return nullptr;
	}

	// NB: cannot create sub component in a thread, otherwise Unreal will complain about pending kill objects still reachable
	auto Actor = GetOwner();
	m_audioComponent = Actor->FindComponentByClass<UAudioComponent>();

	return m_dataDecoder;
}

void UPicoQuicStreamingReaderComp::DestroyDecoder()
{
	m_audioComponent = nullptr;

	m_dataDecoder.reset();
	m_decoderType = DT_Invalid;
}

void UPicoQuicStreamingReaderComp::PrepareRenderer()
{
	if (!Renderer)
		return;

	if (m_decoderType == DT_CortoMesh && OverrideMeshBasedMaterial != nullptr)
	{
		Renderer->SetECVMaterial(OverrideMeshBasedMaterial);
	}

	if (m_decoderType == DT_EvercoastVoxel && OverrideVoxelBasedMaterial != nullptr)
	{
		Renderer->SetECVMaterial(OverrideVoxelBasedMaterial);
	}

	Renderer->ChooseCorrespondingSubRenderer(m_decoderType);
}


void UPicoQuicStreamingReaderComp::SetRendererActor(TSoftObjectPtr<AActor> InActor)
{
	if (InActor.IsNull() || !InActor.IsValid() || InActor->IsDefaultSubobject())
	{
		RendererActor.Reset();
		Renderer.Reset();
		return;
	}

	auto oldRenderer = Renderer;

	RendererActor = InActor;
	if (RendererActor)
	{
		Renderer = RendererActor->FindComponentByClass < UEvercoastRendererSelectorComp >();
		if (Renderer)
		{
			// Make sure it has compatible sub renderer
			if (m_dataDecoder)
			{
				PrepareRenderer();
			}
		}
	}
	else
	{
		Renderer = nullptr;
	}
}

void UPicoQuicStreamingReaderComp::SetRenderer(TSoftObjectPtr<UEvercoastRendererSelectorComp> InRenderer)
{
	auto oldRenderer = Renderer;

	Renderer = InRenderer;
	RendererActor = nullptr; // because we are setting renderer directly, force RendererActor to empty

	if (Renderer)
	{
		// Make sure it has compatible sub renderer
		if (m_dataDecoder)
		{
			PrepareRenderer();
		}
	}
}

TSoftObjectPtr<AActor> UPicoQuicStreamingReaderComp::GetRendererActor() const
{
	return RendererActor;
}

TSoftObjectPtr<UEvercoastRendererSelectorComp> UPicoQuicStreamingReaderComp::GetRenderer() const
{
	return Renderer;
}

#if WITH_EDITOR

void UPicoQuicStreamingReaderComp::StartPreviewInEditor()
{
	Connect(ServerAddress, ServerPort, AuthToken);
}

void UPicoQuicStreamingReaderComp::StopPreviewInEditor()
{
	Disconnect();
}

#endif

void UPicoQuicStreamingReaderComp::Pause(bool pause)
{
	m_dataUploadingPaused = pause;
}

bool UPicoQuicStreamingReaderComp::IsPaused() const
{
	return m_dataUploadingPaused;
}

float UPicoQuicStreamingReaderComp::GetDataReceivingRate()
{
	return (float)m_dataReceivingCounter->GetSampleAverageInt64OnDuration();
}

float UPicoQuicStreamingReaderComp::GetDataDecodingRate()
{
	return (float)m_dataDecodingCounter->GetSampleAverageInt64OnDuration();
}

int UPicoQuicStreamingReaderComp::GetAudioFrameMissing()
{
	return (int)m_audioFrameMissCounter->GetSampleAccumulatedInt64();
}

float UPicoQuicStreamingReaderComp::GetVideoLaggingTime()
{
	return (float)m_videoLaggingCounter->GetSampleAverageDoubleOnCount();
}

float UPicoQuicStreamingReaderComp::GetActualVideoBehindAudioTime()
{
	return (float)m_videoBehindAudioCounter->GetSampleAverageDoubleOnCount();
}

int UPicoQuicStreamingReaderComp::GetVideoDiscardedCount()
{
	return (int)m_videoDiscardCounter->GetSampleAccumulatedInt64();
}

bool UPicoQuicStreamingReaderComp::IsAudioVideoSynced()
{
	if (m_sound)
		return m_sound->HasSynced();

	return false;
}

float UPicoQuicStreamingReaderComp::GetCachedAudioTime()
{
	if (m_sound)
		return m_sound->GetCachedAudioTime();
	return 0;
}


float UPicoQuicStreamingReaderComp::GetSecondaryCachedAudioTime()
{
	if (m_sound)
		return m_sound->GetSecondaryCachedAudioTime();

	return 0;
}

int UPicoQuicStreamingReaderComp::GetCachedVideoFrameCount()
{
	return (int)m_decodedResultQueue.size();
}

std::shared_ptr<GenericDecodeResult> UPicoQuicStreamingReaderComp::PopSynchronisedResultRegardsAudioTimestamp(double audioTimestamp, std::shared_ptr<IEvercoastStreamingDataDecoder> disposer)
{
	// Normal: when same geometry packets arrive later than audio packets, so the current audio timestamp 
	// is usually greater than the current geometry timestamp
	// In that case, we want to pop the geometry result and upload to renderer
	auto it = m_decodedResultQueue.begin();
	while (it != m_decodedResultQueue.end())
	{
		std::shared_ptr<GenericDecodeResult> result = *it;
		double geomTimestamp = result->frameTimestamp;

		double audioGeomTimeDiff = audioTimestamp - geomTimestamp;
		if (audioGeomTimeDiff >= 0 )
		{
			m_decodedResultQueue.pop_front();

#if !SKIP_GEOM_FRAMES
			m_videoLaggingCounter->AddSampleAsDouble(audioGeomTimeDiff);
			return result;
#else			
			// peek the next one
			// if nothing left, or the next frame's timestamp is actually beyond(faster) than audio, then this is the best result
			if (m_decodedResultQueue.empty() || m_decodedResultQueue.front()->frameTimestamp - audioTimestamp > 0)
			{
				m_videoLaggingCounter->AddSampleAsDouble(audioTimestamp - geomTimestamp);
				return result;
			}
			else
			{
				UE_LOG(EvercoastRealtimeLog, Warning, TEXT("Geometry has better candidate. Discard geometry frame(geom: %.2f audio: %.2f)"), geomTimestamp, audioTimestamp);

				// Discard this frame shouldn't introduce stuttering, as there are more decoded frames in the cache
				// dispose result, esp. for voxel data
				disposer->DisposeResult(result);
				m_videoDiscardCounter->AddSample();

				// Try next, there should be a better result 
				++it;
				continue;
			}
#endif
		}
		else
		{
			
			// When geometry data arrives before audio data, when this function is running at a higher frequency of the audio packets arriving, it becomes
			// pretty frequent. So instead of discarding geometry data, let's just wait until the latest audio timestamp catches up geometry's
			//UE_LOG(EvercoastRealtimeLog, Log, TEXT("Geometry is beyond audio: %.2f (audio: %.2f). Wait"), geomTimestamp, audioTimestamp);

			// It will be a negative number but we need that be accurate for delay compensation
			m_videoLaggingCounter->AddSampleAsDouble(audioGeomTimeDiff);

			break;
		}
	}

	return nullptr;
}
