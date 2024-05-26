#include "RuntimeAudio.h"


URuntimeAudio::URuntimeAudio(const FObjectInitializer& ObjectInitializer) :
	USoundWaveProcedural(ObjectInitializer),
	PlaybackCursor(0)
{
	VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
}

void URuntimeAudio::UpdateData(const RuntimeAudioData& data)
{
	// TODO: better replacing PCM data than copying
	AudioData = data;

	{
		std::lock_guard<std::recursive_mutex> guard(PlaybackCursorMutex);
		// crop the playback cursor to the duration
		if (PlaybackCursor > AudioData.GetFrameCount() - 1)
		{
			PlaybackCursor = AudioData.GetFrameCount() - 1;
		}
	}

	Duration = data.Metadata.Duration;
	SetSampleRate(data.Metadata.SampleRate);
	NumChannels = data.Metadata.Channels;
	SoundGroup = SOUNDGROUP_Default;

	if (NumChannels == 4)
	{
		bIsAmbisonics = 1;
	}

	bProcedural = true;
	DecompressionType = EDecompressionType::DTYPE_Procedural;

	RawPCMDataSize = data.PCMData.Num() * data.PCMData.GetTypeSize();
}


int32 URuntimeAudio::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	// Check audio data has been initialised
	if (!AudioData.IsValid())
		return 0;

	std::lock_guard<std::recursive_mutex> guard(PlaybackCursorMutex);
	if (PlaybackCursor > AudioData.GetFrameCount() - 1)
	{
		// Reached the end
		return 0;
	}

	// Trim the actual played samples to avoid out buffer overflow
	int32 ActualNumSamples = NumSamples;
	if (PlaybackCursor + (ActualNumSamples / AudioData.Metadata.Channels) > AudioData.GetFrameCount())
		ActualNumSamples = (AudioData.GetFrameCount() - PlaybackCursor) * AudioData.Metadata.Channels;

	// Assert the actual samples generated is the multiple of channel num
	check(ActualNumSamples % AudioData.Metadata.Channels == 0);

	const float* pData = AudioData.GetDataFromFrame(PlaybackCursor);
	check(pData);

	OutAudio.Append((uint8*)pData, sizeof(float) * ActualNumSamples);

	// Move the cursor
	PlaybackCursor += ActualNumSamples / AudioData.Metadata.Channels;

	return ActualNumSamples;
}

float URuntimeAudio::GetPlaybackTime() const
{
	return (float)PlaybackCursor / (float)AudioData.Metadata.SampleRate;
}

float URuntimeAudio::GetPlaybackProgress() const
{
	return (float)PlaybackCursor / (float)AudioData.GetFrameCount();
}

void URuntimeAudio::SeekToTime(float time)
{
	std::lock_guard<std::recursive_mutex> guard(PlaybackCursorMutex);

	if (!AudioData.IsValid())
	{
		PlaybackCursor = 0;
		return;
	}

	time = std::min(std::max(0.0f, time), AudioData.Metadata.Duration);

	PlaybackCursor = (uint64_t)(time * AudioData.Metadata.SampleRate);

	if (PlaybackCursor > AudioData.GetFrameCount() - 1)
	{
		PlaybackCursor = AudioData.GetFrameCount() - 1;
	}
}

void URuntimeAudio::SeekToFrame(uint64_t frame)
{
	std::lock_guard<std::recursive_mutex> guard(PlaybackCursorMutex);

	if (!AudioData.IsValid())
	{
		PlaybackCursor = 0;
		return;
	}

	PlaybackCursor = frame;

	if (PlaybackCursor > AudioData.GetFrameCount() - 1)
	{
		PlaybackCursor = AudioData.GetFrameCount() - 1;
	}
}
void URuntimeAudio::SeekToProgress(float percent)
{
	std::lock_guard<std::recursive_mutex> guard(PlaybackCursorMutex);

	if (!AudioData.IsValid())
	{
		PlaybackCursor = 0;
		return;
	}

	uint64_t frame = (uint64)(percent * AudioData.GetFrameCount());
	SeekToFrame(frame);
}

#if ENGINE_MAJOR_VERSION < 5
float URuntimeAudio::GetDuration()
#else
float URuntimeAudio::GetDuration() const
#endif
{
	if (!AudioData.IsValid())
		return 0;

	return AudioData.Metadata.Duration;
}


