#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include <mutex>
#include "Sound/SoundWave.h"
#include "Sound/SoundWaveProcedural.h"
#include "Runtime/Launch/Resources/Version.h"
#include "RuntimeAudio.generated.h"

enum RuntimeAudioFormat
{
	WAV,
	MP3,
	UNKNOWN
};

struct RuntimeAudioMetadata
{
	int8_t Channels = 0;
	int32_t SampleRate = 0;
	float Duration = 0.0f;
};

struct RuntimeAudioData
{
	RuntimeAudioMetadata Metadata;
	TArray<float> PCMData;

	bool IsValid() const
	{
		if (PCMData.Num() == 0 ||
			Metadata.Duration == 0 || Metadata.SampleRate == 0 || Metadata.Channels == 0)
			return false;

		return true;
	}

	uint64_t GetFrameCount() const
	{
		return PCMData.Num() / Metadata.Channels;
	}

	const float* GetDataFromFrame(int32_t frameIndex) const
	{
		// Be careful about the unprotected retrieve
		return PCMData.GetData() + frameIndex * Metadata.Channels;
	}
};

UCLASS()
class URuntimeAudio : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	URuntimeAudio(const FObjectInitializer& ObjectInitializer);
	void UpdateData(const RuntimeAudioData& data);

	float GetPlaybackTime() const;
	float GetPlaybackProgress() const;
	void SeekToTime(float time);
	void SeekToFrame(uint64_t frame);
	void SeekToProgress(float percent);

	// USoundWaveProcedural
	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;
	// ~USoundWaveProcedural

	// USoundWave
	virtual float GetDuration()
#if ENGINE_MAJOR_VERSION < 5
		override;
#else
		const override;
#endif

	virtual Audio::EAudioMixerStreamDataFormat::Type GetGeneratedPCMDataFormat() const
	{
		return Audio::EAudioMixerStreamDataFormat::Type::Float;
	}
	// ~USoundWave

private:
	RuntimeAudioData		AudioData;
	uint64_t				PlaybackCursor; // current frame index
	std::recursive_mutex	PlaybackCursorMutex;
};

