#include "Mp3Decoder.h"
#include "RuntimeAudio.h"
#include "dr_mp3.h"


struct DrMp3Destroyer final
{
	drmp3& m_dr_mp3;
	DrMp3Destroyer(drmp3& dr_mp3) :
		m_dr_mp3(dr_mp3)
	{

	}

	~DrMp3Destroyer()
	{
		drmp3_uninit(&m_dr_mp3);
	}
};

bool Mp3Decoder::Validate(const uint8_t* data, int32 dataSize)
{
	drmp3 mp3;

	if (!drmp3_init_memory(&mp3, data, dataSize, nullptr))
	{
		return false;
	}

	DrMp3Destroyer guard(mp3);
	return true;
}

bool Mp3Decoder::Decode(const uint8_t* data, int32_t dataSize, RuntimeAudioMetadata* outMetadata, TArray<float>* outPCMData)
{
	if (!Validate(data, dataSize))
		return false;

	drmp3 dr_mp3;
	if (!drmp3_init_memory(&dr_mp3, data, dataSize, nullptr))
	{
		return false;
	}

	DrMp3Destroyer guard(dr_mp3);

	const uint64_t totalPCMFrameCount = drmp3_get_pcm_frame_count(&dr_mp3);
	// fill in the metadata
	outMetadata->Duration = (float)totalPCMFrameCount / dr_mp3.sampleRate;
	outMetadata->Channels = dr_mp3.channels;
	outMetadata->SampleRate = dr_mp3.sampleRate;

	// fill in the pcm data
	outPCMData->SetNum(totalPCMFrameCount * dr_mp3.channels, true);

	uint64_t framesCount = drmp3_read_pcm_frames_f32(&dr_mp3, totalPCMFrameCount, outPCMData->GetData());
	check(framesCount <= totalPCMFrameCount); // we don't want to overflow the TArray container

	// This might happen if the actual read and transcoded samples are less than declared in the header?
	if (framesCount < totalPCMFrameCount)
	{
		outPCMData->SetNum(framesCount * dr_mp3.channels, true);
	}

	return true;
}
