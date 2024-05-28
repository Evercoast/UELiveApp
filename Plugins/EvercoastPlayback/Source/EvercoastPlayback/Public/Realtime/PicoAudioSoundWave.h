#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "UObject/ObjectMacros.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Queue.h"
#include "Sound/SoundWave.h"
#include <vector>
#include <mutex>
#include <queue>
#include <memory>

#include "PicoAudioSoundWave.generated.h"

#if PLATFORM_IOS
#define DEFAULT_PROCEDURAL_SOUNDWAVE_BUFFER_SIZE (8 * 1024)
#else
#define DEFAULT_PROCEDURAL_SOUNDWAVE_BUFFER_SIZE 1024
#endif

class EvercoastPerfCounter;

DECLARE_LOG_CATEGORY_EXTERN(EvercoastRealtimeAudioLog, Log, All);

DECLARE_DELEGATE_TwoParams(FOnPicoAudioSoundWaveUnderflow, class UPicoAudioSoundWave*, int32);

UCLASS()
class UPicoAudioSoundWave : public USoundWave
{
	GENERATED_BODY()

public:
	UPicoAudioSoundWave(const FObjectInitializer& objectInitializer);
	virtual ~UPicoAudioSoundWave();


	virtual Audio::EAudioMixerStreamDataFormat::Type GetGeneratedPCMDataFormat() const override;

	//~ Begin UObject Interface. 
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	//~ End UObject Interface. 

	//~ Begin USoundWave Interface.
	virtual int32 GeneratePCMData(uint8* PCMData, const int32 SamplesNeeded) override;
	virtual bool HasCompressedData(FName Format, ITargetPlatform* TargetPlatform) const override;
	virtual void BeginGetCompressedData(FName Format, const FPlatformAudioCookOverrides* CompressionOverrides) override;
	virtual FByteBulkData* GetCompressedData(FName Format, const FPlatformAudioCookOverrides* CompressionOverrides = nullptr) override;
	virtual void InitAudioResource(FByteBulkData& CompressedData) override;
	virtual bool InitAudioResource(FName Format) override;
	virtual int32 GetResourceSizeForFormat(FName Format) override;
	//~ End USoundWave Interface.

	// Virtual function to generate PCM audio from the audio render thread. 
	// Returns number of samples generated
	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) { return 0; }

	/** Add data to the FIFO that feeds the audio device. */
	void QueueAudio(double timestamp, uint64 frameNum, const uint8* headerData, const uint64 headerDataSize, const uint8* AudioData, const uint64 AudioDataSize);

	void Sync(double timestamp);

	/** Remove all queued data from the FIFO. This is only necessary if you want to start over, or GeneratePCMData() isn't going to be called, since that will eventually drain it. */
	void ResetAudio();

	/** Size in bytes of a single sample of audio in the procedural audio buffer. */
	int32 SampleByteSize;

	double GetLastFedPCMTimestamp() const;

	void SetMissingFrameCounter(std::shared_ptr<EvercoastPerfCounter> counter);

	bool HasSynced() const
	{
		return m_hasInitialSynced && m_audioBufferPumpDelay < 0.5;
	}

	void SetAudioBufferDelay(double delayInSeconds);

	void Tick(float DeltaTime);

private:
	struct AudioSegment
	{
		int64_t frame_num;
		double timestamp;
		double duration;
		TArray<uint8> pcm;

		// Linear interpolate from v0 to this, based on provided value t which ranges from 0 to 1
		AudioSegment lerp(const AudioSegment& v0, float t) const;
	};


	// Pumps audio queued from game thread
	void PumpQueuedAudio();

	bool InterpolateAudio(const AudioSegment& newSegment, int interpolatingFrame, AudioSegment& out);

	

	mutable std::mutex m_mutex{};
	std::queue<AudioSegment> m_segments{};
	AudioSegment m_lastReceivedSegment{ -1, -1, -1 };
	
	// Flag to reset the audio buffer
	FThreadSafeBool bReset;
	// Only accessible in audio thread, no need to lock m_audioBuffer
	TArray<uint8> m_audioBuffer;

	bool m_initialised{ false };
	FThreadSafeBool m_isReady{ false };
	FThreadSafeBool m_hasInitialSynced{ false };

	uint64 m_prevFrameNum{ 0 };
	double m_initialTimestamp{ 0 };

	mutable std::mutex m_stats_mutex{};
	double m_lastAudioBufferTimestamp{ 0 };
	double m_lastPCMGenerationFedTimestamp{ 0 };

	std::shared_ptr<EvercoastPerfCounter> m_missingFrameCounter;

	double m_audioBufferPumpDelay{ 0 };
	double m_currVideoSyncingExtrapolatedTimestamp{ 0 };
};
