#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "picoquic.h"
#include "EvercoastPerfCounter.h"
#include <memory>
#include <queue>
#include <functional>
#include "Delegates/Delegate.h"
#include "PicoQuicStreamingReaderComp.generated.h"

class UEvercoastVoxelRendererComp;
class IEvercoastStreamingDataDecoder;
class IGenericDecoder;
class UAudioComponent;
class RealtimeNetworkThread;
class UPicoAudioSoundWave;
enum DecoderType : uint8;
class GenericDecodeResult;

class UEvercoastRendererSelectorComp;

DECLARE_LOG_CATEGORY_EXTERN(EvercoastRealtimeLog, Log, All);

static_assert((uint8)PicoQuic::Status::NotYetConnected == 0, "PicoQuic constant setup error!");

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPicoQuicStreamingReaderaComp_OnConnectionSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPicoQuicStreamingReaderComp_OnConnectionFailure, FString, FailureReason);

UENUM(BlueprintType)
enum class EPicoQuicStatus : uint8
{
	NotYetConnected = 0, // PicoQuic::Status::NotYetConnected, // Make Unreal Header Tool happy
	Connected = (uint8)PicoQuic::Status::Connected,
	FailedToConnect = (uint8)PicoQuic::Status::FailedToConnect,
	Disconnected = (uint8)PicoQuic::Status::Disconnected,
	ProtocolError = (uint8)PicoQuic::Status::ProtocolError,
	HandleIsInvalid = (uint8)PicoQuic::Status::HandleIsInvalid,
	FailedToAuthenticate = (uint8)PicoQuic::Status::FailedToAuthenticate
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class EVERCOASTPLAYBACK_API UPicoQuicStreamingReaderComp : public UActorComponent
{
	GENERATED_BODY()
public:
	UPicoQuicStreamingReaderComp(const FObjectInitializer& initializer);
	virtual ~UPicoQuicStreamingReaderComp();

	UFUNCTION(BlueprintCallable, Category = "Livestreaming")
	void Connect(const FString& serverName, int port, const FString& accessToken);

	UFUNCTION(BlueprintCallable, Category = "Livestreaming")
	void ConnectExpress();

	UFUNCTION(BlueprintCallable, Category = "Livestreaming")
	void Disconnect();

	UFUNCTION(BlueprintCallable, Category = "Livestreaming")
	bool IsConnected() const;

	UFUNCTION(BlueprintCallable, Category = "Livestreaming")
	void Pause(bool pause);

	UFUNCTION(BlueprintCallable, Category = "Livestreaming")
	bool IsPaused() const;

	UFUNCTION(BlueprintCallable, Category = "Livestreaming")
	EPicoQuicStatus GetStatus() const;

	UFUNCTION(BlueprintCallable, Category = "Livestreaming")
	bool IsStatusGood() const;

	UFUNCTION(BlueprintCallable, Category = "Profiling")
	float GetDataReceivingRate();

	UFUNCTION(BlueprintCallable, Category = "Profiling")
	float GetDataDecodingRate();

	// Blueprint only supports int32
	UFUNCTION(BlueprintCallable, Category = "Profiling")
	int GetAudioFrameMissing();

	UFUNCTION(BlueprintCallable, Category = "Profiling")
	float GetVideoLaggingTime();

	UFUNCTION(BlueprintCallable, Category = "Profiling")
	int GetVideoDiscardedCount();

	UFUNCTION(BlueprintCallable, Category = "Profiling")
	bool IsAudioVideoSynced();

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Livestreaming")
	void StartPreviewInEditor();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Livestreaming")
	void StopPreviewInEditor();
#endif
	UPROPERTY(VisibleDefaultsOnly, AdvancedDisplay, Category = "Livestreaming")
	TSoftObjectPtr<UEvercoastRendererSelectorComp> Renderer;

	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, BlueprintSetter = SetRendererActor, BlueprintGetter = GetRendererActor, Category = "Livestreaming")
	TSoftObjectPtr<AActor> RendererActor;

public:
	// Blueprint functions
	UFUNCTION(BlueprintCallable, Category = "Livestreaming")
	void SetRendererActor(TSoftObjectPtr<AActor> InActor);

	UFUNCTION(BlueprintCallable, Category = "Livestreaming")
	TSoftObjectPtr<AActor> GetRendererActor() const;

	// ~Overrides
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void OnStreamTimer();

	// Set renderer directly without involving an actor, ignores the auto created renderer
	void SetRenderer(TSoftObjectPtr<UEvercoastRendererSelectorComp> InRenderer);

	// Get renderer directly
	TSoftObjectPtr<UEvercoastRendererSelectorComp> GetRenderer() const;

private:
	std::shared_ptr<IEvercoastStreamingDataDecoder> CreateDecoder(uint32_t stream_type);
	void DestroyDecoder();
	void PrepareRenderer();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livestreaming")
	FString		ServerAddress;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livestreaming")
	int			ServerPort;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livestreaming")
	FString		CertificationPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livestreaming")
	FString		AuthToken;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livestreaming")
	bool		AutoConnect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livestreaming")
	bool		DebugLogging;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livestreaming")
	bool		IgnoreAudio;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livestreaming")
	UMaterialInterface* OverrideVoxelBasedMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livestreaming")
	UMaterialInterface* OverrideMeshBasedMaterial;

	UPROPERTY(BlueprintAssignable)
	FPicoQuicStreamingReaderComp_OnConnectionFailure OnConnectionFailure;

	UPROPERTY(BlueprintAssignable)
	FPicoQuicStreamingReaderaComp_OnConnectionSuccess OnConnectionSuccess;
private:

	std::shared_ptr<GenericDecodeResult>				PopSynchronisedResultRegardsAudioTimestamp(double timestamp, std::shared_ptr<IEvercoastStreamingDataDecoder> disposer);

	std::shared_ptr<RealtimeNetworkThread>				m_networkThread;
	std::shared_ptr<IEvercoastStreamingDataDecoder>		m_dataDecoder;
	UAudioComponent*									m_audioComponent;
	UPicoAudioSoundWave*								m_sound;

	FTimerHandle										m_StreamTimer;
	uint64_t											m_SampledFrameNumStart;
	uint64_t											m_SampledFrameNumEnd;
	DecoderType											m_decoderType;

	bool												m_dataUploadingPaused;

	std::shared_ptr<EvercoastPerfCounter>				m_dataReceivingCounter;
	std::shared_ptr<EvercoastPerfCounter>				m_dataDecodingCounter;
	std::shared_ptr<EvercoastPerfCounter>				m_audioFrameMissCounter;
	std::shared_ptr<EvercoastPerfCounter>				m_videoLaggingCounter;
	std::shared_ptr<EvercoastPerfCounter>				m_videoDiscardCounter;

	std::deque<std::shared_ptr<GenericDecodeResult>>	m_decodedResultQueue;

	std::function<void(void)>							m_onConnectionSuccessCallback;
	std::function<void(FString)>						m_onConnectionFailureCallback;
};
