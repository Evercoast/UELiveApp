// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GhostTreeFormatReader.h"
#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "Components/ActorComponent.h"
#include "EvercoastECVAsset.h"
#include "GenericDecoder.h"
#include "Math/Range.h"
#include <future>
#include <mutex>
#include <numeric>
#include "UObject/LazyObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "TimestampDriver.h"
#include "Engine/Texture.h"
#include "EvercoastStreamingReaderComp.generated.h"

class UAudioComponent;
class IEvercoastStreamingDataDecoder;
class UEvercoastVoxelRendererComp;
class UCortoMeshRendererComp;
class TheReaderDelegate;
class UVideoTextureHog;
class UTexture2D;
class UMediaPlayer;
class UEvercoastMediaSoundComp;
class UEvercoastRendererSelectorComp;
class UGhostTreeFormatReader;
struct FECVAssetPersistentData;
struct FECVAssetEvalTemplate;
struct FECVAssetTrackSectionParams;
class AEvercoastVolcapActor;

UENUM(BlueprintType)
enum class EVideoCodecProvider: uint8
{
	AUTO = 0,
	BUILTIN,
	FFMPEG
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class EVERCOASTPLAYBACK_API UEvercoastStreamingReaderComp : public UActorComponent, public EvercoastStreamingReaderStatusCallback
{
	GENERATED_BODY()

	friend struct FECVAssetPersistentData;
public:	
	// Sets default values for this component's properties
	UEvercoastStreamingReaderComp(const FObjectInitializer& ObjectInitializer);
	virtual ~UEvercoastStreamingReaderComp();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Source", BlueprintSetter = SetECVAsset )
	UEvercoastECVAsset* ECVAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Source")
	bool bAsyncDataDecoding;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Source")
	int32 MaxCacheSizeInMB = 1024; // 1G cache

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Source")
	bool bPreferVideoCodec;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Source")
	UTexture* DebugPlaceholderTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Source", meta = (EditCondition = "bPreferVideoCodec" ))
	EVideoCodecProvider VideoCodecProvider = EVideoCodecProvider::AUTO;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playback")
	bool bAutoPlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playback")
	bool bLoop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playback")
	bool bSlackTiming;

	UPROPERTY(VisibleDefaultsOnly, AdvancedDisplay, Category = "Rendering")
	UEvercoastRendererSelectorComp* Renderer;

	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, BlueprintSetter = SetRendererActor, BlueprintGetter = GetRendererActor, Category = "Rendering")
	AEvercoastVolcapActor* RendererActor;

	UPROPERTY(EditAnywhere, Category = "Data Source", meta = (Tooltip="Limit the source data's bit rate. Unit is megabit per second.", GetOptions = "GetDataBitRates"))
	FString DataBitRateLimit = "Unlimited";

	UPROPERTY(EditAnywhere, Category = "Data Source", meta = (Tooltip = "Choose the frame rate(sample rate) of the volumetric capture.", GetOptions = "GetAvailableFrameRates"))
	FString DesiredFrameRate = "Highest";

	UPROPERTY(EditAnywhere, Category = "Data Source", meta = (Tooltip = "Normally disk cache will be used. Turn on this option to force using memory cache. On some device like iOS which memory is limited this option should be left off."))
	bool bForceMemoryCache = false;

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	void StreamingPlay();

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	void StreamingPause();

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	void StreamingResume();

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	void StreamingStop();

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	void StreamingSeekTo(float timestamp);

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	void StreamingJump(float deltaTime);

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	void StreamingPrevFrame();

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	void StreamingNextFrame();

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	float StreamingGetCurrentTimestamp() const;

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	int32 StreamingGetCurrentFrameNumber() const;

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	float StreamingGetCurrentSeekingTarget() const;

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	float StreamingGetDuration() const;

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	bool IsStreamingPlaying() const;

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	bool IsStreamingPaused() const;

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	bool IsStreamingStopped() const;

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	bool IsStreamingPendingSeek() const;

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	bool IsStreamingWaitingForData() const;

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	bool IsStreamingWaitingForAudioData() const;

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	bool IsStreamingPlaybackReady() const;

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	bool IsStreamingPlayableWithoutBlocking() const;

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	void SetPlaybackStartTime(float startTimestamp);

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	float GetPlaybackStartTime() const;

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	void SetPlaybackMicroTimeManagement(float overrideCurrTime, float overrideDeltaTime, float blockTimestamp); // for Sequencer micro time management

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	void RemovePlaybackMicroTimeManagement(); // for Sequencer micro time management

	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetRendererActor(AEvercoastVolcapActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "Data Source")
	void SetECVAsset(UEvercoastECVAsset* asset);

	UFUNCTION(BlueprintCallable, Category = "Rendering")
	AEvercoastVolcapActor* GetRendererActor() const;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Data Source")
	void RecreateReader();

	UFUNCTION()
	TArray<FString> GetDataBitRates() const;

	UFUNCTION()
	TArray<FString> GetAvailableFrameRates() const;

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	bool HasReaderFatalError() const;

	UFUNCTION(BlueprintCallable, Category = "Evercoast Playback")
	FString GetReaderFatalError();

	// ResetReader then CreateReader then wait till OnOpen callback get invoked, public
	void RecreateReaderSync();

	// ~EvercoastStreamingReaderStatusCallback
	virtual void OnWaitingForDataChanged(bool isWaitingForData) override;
	virtual void OnInSeekingChanged(bool isInSeeking) override;
	virtual void OnPlaybackReadyChanged(bool isPlaybackReady) override;
	virtual void OnFatalError(const char* error) override;
	virtual void OnWaitingForAudioDataChanged(bool isWaitingForAudioData) override;
	virtual void OnWaitingForVideoDataChanged(bool isWaitingForVideoData) override;
	// ~EvercoastStreamingReaderStatusCallback

	// This function is added because of a compromise between async requirement and accurate of seeking in Sequencer
	bool IsStreamingDurationReliable() const;
	void WaitForDurationBecomesReliable();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//~ Begin UActorComponent interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UActorComponent interface

public:	
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	friend class TheReaderDelegate;
	friend struct FECVAssetEvalTemplate;
	friend struct FECVAssetTrackSectionParams;
	friend struct FECVAssetExecutionToken;
	friend struct FECVAssetPreRollExecutionToken;

	void OnReaderEvent(ECReaderEvent event);
	void NotifyReceivedTimestamp(float TimeStamp);
	void NotifyReceivedChannelsInfo();
	bool OnOpenConnection(bool prerequisitySucceeded);
	bool OnCloseConnection(bool prerequisitySucceeded);
	bool IsReaderPlayableWithoutBlocking() const;
	void ToggleAuxPlaybackIfPossible(bool play);
	void ToggleVideoTextureHogIfPossible(bool play);
	void SetPlaybackTiming(bool play);
	float GetPlaybackTiming() const;

	// ~Begin of functions for Sequencer and editors
	void RecalcequencerOverrideLoopCount();
	// ~End of functions for Sequencer and editors

	// ~Functions only for corto mesh decoder
	UTexture* FindVideoTexture(int64_t frameIndex);
	bool InvalidateVideoTexture(UTexture* texture);
	bool InvalidateVideoTextureBeforeFrameIndex(int64_t frameIndex);
	bool IsVideoFrameWithinHogCache(int64_t frameIndex);
	bool IsVideoFrameBeyondHogCache(int64_t frameIndex);
	bool IsVideoFrameBeforeHogCache(int64_t frameIndex);
	bool IsVideoTextureHogFull() const;
	// ~Functions only for corto mesh decoder

	void _PrintDebugStatus() const; 


private:

	enum PlaybackStatus
	{
		Stopped = 0,
		Playing,
		Paused,
	};

	enum SyncStatus
	{
		InSync = 0,
		InSync_SkippedFrames,
		VideoFeedStarving,
		GeomFeedStarving
	};

	void CreateReader();
	void ResetReader();
	void DoRefreshRenderer();

	UPROPERTY(Transient)
	UGhostTreeFormatReader* m_reader;

	std::shared_ptr<IEvercoastStreamingDataDecoder> m_dataDecoder;
	std::shared_ptr<IGenericDecoder> m_baseDecoder;
	std::shared_ptr<IGenericDecoder> m_auxDecoder;

	float m_currReadTimestamp;

	PlaybackStatus m_playbackStatus;

	/* Auxillary objects for GT container streamed audio*/
	UAudioComponent* m_audioComponent;

	bool m_isReaderWaitingForData;
	bool m_isReaderInSeeking;
	bool m_isReaderPlaybackReady;
	bool m_isReaderWaitingForAudioData;
	bool m_isReaderWaitingForVideoData;

	/* ~Start of auxillary objects for VIDEO container streamed video/audio*/
	UPROPERTY(Transient)
	UVideoTextureHog* m_videoTextureHog;

	UPROPERTY(Transient)
	UMediaPlayer* m_mediaSoundPlayer;
	/* ~End of auxillary objects for VIDEO container streamed video/audio*/

	SyncStatus m_syncStatus;

	// Debug purpose
	int64_t m_lastMismatchedFrame;
	int m_consecutiveMismatchedFrameCount;

	bool bOverrideUpdate;
	float m_playbackInitSeek;

	std::promise<void>	m_fileOpenPromise;
	std::future<void> m_fileOpenFuture;


	TSharedPtr<FTimestampDriver, ESPMode::ThreadSafe> m_timestampDriver;
	float m_geomFeedStarvingStartTime;

	enum CortoTextureSeekStage
	{
		CTS_NA,			// voxel, or doesn't need external video data
		CTS_DEFAULT,	// default state
		CTS_REQUESTED,	// requested video seek
		CTS_COMPLETED	// completed video seek
	};
	CortoTextureSeekStage m_cortoTexSeekStage;

	bool m_readerHasFatalError;
	FString m_readerFataErrorMessage;
	int32_t m_currentMatchingFrameNumber;
	float m_currentMatchingTimestamp;
};
