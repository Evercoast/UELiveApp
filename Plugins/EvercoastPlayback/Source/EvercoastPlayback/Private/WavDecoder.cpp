#include "WavDecoder.h"
#include "dr_wav.h"
#include "RuntimeAudio.h"

struct DrWavDestroyer final
{
	drwav& m_dr_wav;
	DrWavDestroyer(drwav& dr_wav) :
		m_dr_wav(dr_wav)
	{

	}

	~DrWavDestroyer()
	{
		drwav_uninit(&m_dr_wav);
	}
};

bool WavDecoder::ValidateSimple(const uint8_t* data, int32_t dataSize)
{
	drwav dr_wav;

	if (!drwav_init_memory(&dr_wav, data, dataSize, nullptr))
	{
		return false;
	}

	DrWavDestroyer guard(dr_wav);
	return true;
}


bool WavDecoder::ValidateAndFixDuration(uint8_t* data, int32_t dataSize)
{
	drwav dr_wav;

	// Initializing transcoding of audio data in memory
	if (!drwav_init_memory(&dr_wav, data, dataSize, nullptr))
	{
		return false;
	}

	DrWavDestroyer guard(dr_wav);
	// Check if the container is RIFF (not Wave64 or any other containers)
	if (dr_wav.container != drwav_container_riff)
	{
		return true;
	}

	// Get 4-byte field at byte 4, which is the overall file size as uint32, according to RIFF specification.
	// If the field is set to nothing (hex FFFFFFFF), replace the incorrectly set field with the actual size.
	// The field should be (size of file - 8 bytes), as the chunk identifier for the whole file (4 bytes spelling out RIFF at the start of the file), and the chunk length (4 bytes that we're replacing) are excluded.
	if (BytesToHex(data + 4, 4) == "FFFFFFFF")
	{
		const int32 ActualFileSize = dataSize - 8;
		FMemory::Memcpy(data + 4, &ActualFileSize, 4);
	}

	// Search for the place in the file after the chunk id "data", which is where the data length is stored.
	// First 36 bytes are skipped, as they're always "RIFF", 4 bytes filesize, "WAVE", "fmt ", and 20 bytes of format data.
	uint32 DataSizeLocation = INDEX_NONE;
	for (uint32 Index = 36; Index < static_cast<uint32>(dataSize) - 4; ++Index)
	{
		// "64617461" - hex for string "data"
		if (BytesToHex(data + Index, 4) == "64617461")
		{
			DataSizeLocation = Index + 4;
			break;
		}
	}

	// Should never happen, but just in case
	if (DataSizeLocation == INDEX_NONE)
	{
		return false;
	}

	// Same process as replacing full file size, except DataSize counts bytes from end of DataSize int to end of file.
	if (BytesToHex(data + DataSizeLocation, 4) == "FFFFFFFF")
	{
		// -4 to not include the DataSize int itself
		const uint32 ActualDataSize = dataSize - DataSizeLocation - 4;

		FMemory::Memcpy(data + DataSizeLocation, &ActualDataSize, 4);
	}

	return true;
}

bool WavDecoder::Decode(const uint8_t* data, int32_t dataSize, RuntimeAudioMetadata* outMetadata, TArray<float>* outPCMData)
{
	if (!outMetadata || !outPCMData)
		return ValidateSimple(data, dataSize);

	drwav dr_wav;
	if (!drwav_init_memory(&dr_wav, data, dataSize, nullptr))
	{
		return false;
	}

	DrWavDestroyer guard(dr_wav);
	// fill in the metadata
	outMetadata->Duration = (float)dr_wav.totalPCMFrameCount / dr_wav.sampleRate;
	outMetadata->Channels = dr_wav.channels;
	outMetadata->SampleRate = dr_wav.sampleRate;

	// fill in the pcm data
	outPCMData->SetNum(dr_wav.totalPCMFrameCount * dr_wav.channels, true);

	uint64_t framesCount = drwav_read_pcm_frames_f32(&dr_wav, dr_wav.totalPCMFrameCount, outPCMData->GetData());
	check(framesCount <= dr_wav.totalPCMFrameCount); // we don't want to overflow the TArray container
	
	// This might happen if the actual read and transcoded samples are less than declared in the header?
	if (framesCount < dr_wav.totalPCMFrameCount)
	{
		outPCMData->SetNum(framesCount * dr_wav.channels, true);
	}

	return true;
}