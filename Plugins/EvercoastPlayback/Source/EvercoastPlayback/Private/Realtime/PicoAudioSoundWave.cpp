#include "Realtime/PicoAudioSoundWave.h"
#include "EvercoastPerfCounter.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include <fstream>
#include <string>

DEFINE_LOG_CATEGORY(EvercoastRealtimeAudioLog);

UPicoAudioSoundWave::UPicoAudioSoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bProcedural = true;
	bReset = false;

	SampleByteSize = 2;

	VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
}

UPicoAudioSoundWave::~UPicoAudioSoundWave()
{
}

Audio::EAudioMixerStreamDataFormat::Type UPicoAudioSoundWave::GetGeneratedPCMDataFormat() const
{
	return Audio::EAudioMixerStreamDataFormat::Int16;
}

void UPicoAudioSoundWave::QueueAudio(double timestamp, uint64 frameNum, const uint8* headerData, const uint64 headerDataSize, const uint8* pcmData, const uint64 pcmDataSize)
{
	if (!headerData || headerDataSize < 6)
	{
		return;
	}
	const int inSampleFormat = static_cast<uint32>(headerData[0]);
	if (inSampleFormat != 16)
	{
		return;
	}

	const uint32 inNumChannels = static_cast<uint32>(headerData[1]);
	uint32 inSampleRate = 0;
	FMemory::Memcpy(&inSampleRate, headerData + 2, sizeof(uint32));

	int64_t frameNumDiff = 1;
	if (!m_initialised)
	{
		if (inSampleRate == 0)
		{
			return;
		}
		NumChannels = inNumChannels;
		SampleByteSize = 2;
		SetSampleRate(inSampleRate);
		m_initialTimestamp = timestamp;
		m_initialised = true;
	} 
	else if(inNumChannels != NumChannels || inSampleRate != SampleRate)
	{
		UE_LOG(EvercoastRealtimeAudioLog, Warning, TEXT("Realtime streaming audio format changed."));
		return;
	}
	else
	{
		frameNumDiff = static_cast<int64_t>(frameNum) - static_cast<int64_t>(m_prevFrameNum);
	}

	if (pcmDataSize == 0 || !ensure((pcmDataSize % SampleByteSize) == 0))
	{
		return;
	}

	if (frameNumDiff < 0)
	{
		UE_LOG(EvercoastRealtimeAudioLog, Warning, TEXT("Received audio packet in wrong order: %d -> %d (Are you on simulator?)"), m_prevFrameNum, frameNum);
	}

	m_prevFrameNum = frameNum;

	

	if (frameNumDiff > 0 && frameNumDiff < 10)
	{
		double duration = static_cast<double>(pcmDataSize) / (SampleByteSize * NumChannels * SampleRate);

		TArray<uint8> newAudioBuffer;
		newAudioBuffer.AddUninitialized(pcmDataSize);
		FMemory::Memcpy(newAudioBuffer.GetData(), pcmData, pcmDataSize);

		AudioSegment newAudioSegment = AudioSegment{
			(int64_t)frameNum,
			timestamp,
			duration,
			newAudioBuffer
		};

		float lastReceivedSegmentTimestamp;
		{
			std::lock_guard lock(m_mutex);

			// NOTE: Save the last packet and linear interpolate the from the last -> newest packet
			for (uint32_t i = 0; i < frameNumDiff; ++i)
			{
				AudioSegment interpolatedSegment{ -1, -1, -1 };
				if (InterpolateAudio(newAudioSegment, i, interpolatedSegment))
				{
					m_segments.push(interpolatedSegment);
				}
				else
				{
					// No way to interpolate, has to repeat the latest segment
					m_segments.push(newAudioSegment);
				}
			}

			// Save the last segment
			m_lastReceivedSegment = newAudioSegment;
			lastReceivedSegmentTimestamp = newAudioSegment.timestamp;
		}

		{
			std::lock_guard lock2(m_stats_mutex);
			m_lastReceivedAudioTimestamp = lastReceivedSegmentTimestamp;
		}

	}
	else if (frameNumDiff >= 10)
	{
		// Clear audio buffer and re-sync audio/video
		ResetAudio();
	}


	if (frameNumDiff > 1)
	{
		m_missingFrameCounter->AddSampleAsInt64(frameNumDiff - 1);
	}

}

// Precise method, which guarantees v = v1 when t = 1. This method is monotonic only when v0 * v1 < 0.
// Lerping between same values might not produce the same value
static float lerp(float v0, float v1, float t) {
	return (1 - t) * v0 + t * v1;
}

UPicoAudioSoundWave::AudioSegment UPicoAudioSoundWave::AudioSegment::lerp(const AudioSegment& v0, float t) const
{
	AudioSegment out;
	out.frame_num = (uint64_t)::lerp((float)v0.frame_num, (float)this->frame_num, t);
	out.timestamp = (double)::lerp((float)v0.timestamp, (float)this->timestamp, t);
	out.duration = (double)::lerp((float)v0.duration, (float)this->duration, t);

	int32_t pcmDataCount = ::lerp((float)v0.pcm.Num(), (float)this->pcm.Num(), t);
	int32_t remainder = pcmDataCount % 2;
	pcmDataCount -= remainder;
	check(pcmDataCount % 2 == 0);

	for (int32_t i = 0; i < pcmDataCount; i+= 2)
	{
		int v0_idx = (int)::lerp(0, v0.pcm.Num(), (float)i / pcmDataCount);
		int v0_idx_remainder = v0_idx % 2;
		v0_idx -= v0_idx_remainder;

		int v1_idx = (int)::lerp(0, this->pcm.Num(), (float)i / pcmDataCount);
		int v1_idx_remainder = v1_idx % 2;
		v1_idx -= v1_idx_remainder;

		if (v0_idx + 1 >= v0.pcm.Num() || v1_idx + 1 >= this->pcm.Num())
		{
			continue;
		}

		int16_t v0_val = v0.pcm[v0_idx] + (v0.pcm[v0_idx + 1] << 8);
		int16_t v1_val = this->pcm[v1_idx] + (this->pcm[v1_idx + 1] << 8);

		int16_t interpolated_val = ::lerp(v0_val, v1_val, t);
		out.pcm.Add(interpolated_val & 0xff);
		out.pcm.Add(interpolated_val >> 8);
	}
	return out;
}

bool UPicoAudioSoundWave::InterpolateAudio(const AudioSegment& newSegment, int interpolatingFrame, AudioSegment& out)
{
	if (m_lastReceivedSegment.frame_num >= 0 && m_lastReceivedSegment.duration > 0)
	{
		int totalFrameDiff = newSegment.frame_num - m_lastReceivedSegment.frame_num;
		if (totalFrameDiff == 1)
		{
			// shortcut, no need to interpolate
			out = newSegment;
			return true;
		}
		double t = (double)(interpolatingFrame + 1) / totalFrameDiff;

		out = newSegment.lerp(m_lastReceivedSegment, t);
		return true;
	}


	// invalid base
	return false;
}

void UPicoAudioSoundWave::PumpQueuedAudio()
{
	double lastAudioBufferTimestamp = 0;
	bool hasPumpedAudio = false;

	{
		std::lock_guard lock(m_mutex);
		while (!m_segments.empty())
		{
			const AudioSegment& segment = m_segments.front();

			// Once the audio buffer delay is set to a positive value, the audio/geom difference should quickly converge to almost zero,
			// because the audio timestamp is calculated from actually bits fed to audio PCM buffer. There's no enough audio buffer, the 
			// audio timestmap won't progress. There seems no need to put m_audioBufferPumpDelay into the equation
			if (m_audioBufferPumpDelay > 0 && segment.timestamp /* + segment.duration*/ > m_currVideoSyncingExtrapolatedTimestamp)// + m_audioBufferPumpDelay)
			{
				break;
			}

			m_audioBuffer.Append(m_segments.front().pcm);

			lastAudioBufferTimestamp = m_segments.front().timestamp;
			hasPumpedAudio = true;

			m_segments.pop();
		}
	}

	if (hasPumpedAudio)
	{
		std::lock_guard lock(m_stats_mutex);
		m_lastAudioBufferTimestamp = lastAudioBufferTimestamp;
	}
}

int32 UPicoAudioSoundWave::GeneratePCMData(uint8* PCMData, const int32 SamplesNeeded)
{
	// Check if we've been told to reset our audio buffer
	if (bReset)
	{
		bReset = false;
		m_audioBuffer.Reset();
	}

	
	if (m_hasInitialSynced)
	{
		PumpQueuedAudio();

		Audio::EAudioMixerStreamDataFormat::Type Format = GetGeneratedPCMDataFormat();
		SampleByteSize = (Format == Audio::EAudioMixerStreamDataFormat::Int16) ? 2 : 4;

		int32 SamplesAvailable = m_audioBuffer.Num() / SampleByteSize;

		// Wait until we have enough samples that are requested before starting.
		if (SamplesAvailable > 0)
		{
			const int32 SamplesToCopy = FMath::Min<int32>(SamplesNeeded, SamplesAvailable);
			const int32 BytesToCopy = SamplesToCopy * SampleByteSize;

			if (SamplesAvailable < SamplesNeeded)
			{
				UE_LOG(EvercoastRealtimeAudioLog, Warning, TEXT("Audio buffer left over elements: %d PCM buffer needs: %d. Will fill the rest with 0s"), m_audioBuffer.Num(), SamplesNeeded * SampleByteSize);
			}
			
			FMemory::Memcpy((void*)PCMData, &m_audioBuffer[0], BytesToCopy);
			m_audioBuffer.RemoveAt(0, BytesToCopy, false);

			if (SamplesAvailable < SamplesNeeded)
			{
				// Fill the reset with zeros
				FMemory::Memzero(PCMData + BytesToCopy, (SamplesNeeded - SamplesToCopy) * SampleByteSize);
			}

			//check(BytesToCopy + (SamplesNeeded - SamplesToCopy) * SampleByteSize == SamplesNeeded * SampleByteSize);

			{
				// update this important matrics needs lock
				std::lock_guard lock(m_stats_mutex);

				// This should be the last sample sent to the hardware(or the lowest level of abstraction we can get) audio buffer
				const int SamplesLeft = SamplesAvailable - SamplesToCopy;
				m_lastPCMGenerationFedTimestamp = m_lastAudioBufferTimestamp - ((double)SamplesLeft / (NumChannels * SampleRate));
			}

			// Should return the bytes written not the samples!
			return SamplesNeeded * SampleByteSize;
		}
		else
		{
			UE_LOG(EvercoastRealtimeAudioLog, Warning, TEXT("Zero available samples. Audio buffer starving. Will fill with 0s"));

			// Fill the reset with zeros
			int32 ActualCopiedSize = SamplesNeeded * SampleByteSize;
			FMemory::Memzero(PCMData, ActualCopiedSize);

			return ActualCopiedSize;
		}
	} 
	else if(!m_isReady)
	{
		Audio::EAudioMixerStreamDataFormat::Type Format = GetGeneratedPCMDataFormat();
		SampleByteSize = (Format == Audio::EAudioMixerStreamDataFormat::Int16) ? 2 : 4;

		// Warm up
		PumpQueuedAudio();

		if (m_audioBuffer.Num() >= (int32)(m_warmupTime * NumChannels * SampleRate * SampleByteSize))
		{
			m_isReady = true;
		}
	}

	return 0;
}

void UPicoAudioSoundWave::Tick(float DeltaTime)
{
	if (!m_isReady)
	{
		return;
	}

	if (m_hasInitialSynced)
	{
		m_currVideoSyncingExtrapolatedTimestamp += DeltaTime;
	}
}

void UPicoAudioSoundWave::Sync(double timestamp)
{
	
	if (!m_isReady)
	{
		return;
	}

	if (m_hasInitialSynced)
	{
		// m_currVideoSyncingTimestamp needs to be extrapolated, because this huge 15 Hz geom vs 50 Hz audio frequency difference
		m_currVideoSyncingExtrapolatedTimestamp = timestamp;

		return;
	}

	std::lock_guard lock(m_mutex);
	while (!m_segments.empty())
	{
		const auto& entry = m_segments.front();
		if (timestamp > (entry.timestamp + entry.duration))
		{
			UE_LOG(EvercoastRealtimeAudioLog, Log, TEXT("Discard sound during Sync(): %.3f > (%.3f + %.3f)"), timestamp, entry.timestamp, entry.duration);
			m_segments.pop();
		}
		else
		{
			break;
		}
	}

	if (!m_segments.empty() && timestamp >= m_initialTimestamp)
	{
		m_hasInitialSynced = true;
	}
}

void UPicoAudioSoundWave::ResetAudio()
{
	{
		std::lock_guard lock(m_mutex);
		m_hasInitialSynced = false;
		m_audioBufferPumpDelay = 0;
		m_currVideoSyncingExtrapolatedTimestamp = 0;
		m_segments = std::queue<AudioSegment>();
		m_lastReceivedSegment = AudioSegment{ -1, -1, -1 };
		m_initialTimestamp = 0;
		m_audioBuffer.Reset();

		m_initialised = false;
		m_isReady = false;
		bReset = true;
	}

	{
		std::lock_guard lock(m_stats_mutex);
		m_lastAudioBufferTimestamp = 0;
		m_lastPCMGenerationFedTimestamp = 0;
		m_lastReceivedAudioTimestamp = 0;
	}
}

int32 UPicoAudioSoundWave::GetResourceSizeForFormat(FName Format)
{
	return 0;
}

void UPicoAudioSoundWave::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
}

bool UPicoAudioSoundWave::HasCompressedData(FName Format, ITargetPlatform* TargetPlatform) const
{
	return false;
}

void UPicoAudioSoundWave::BeginGetCompressedData(FName Format, const FPlatformAudioCookOverrides* CompressionOverrides)
{
	// SoundWaveProcedural does not have compressed data and should generally not be asked about it
}

FByteBulkData* UPicoAudioSoundWave::GetCompressedData(FName Format, const FPlatformAudioCookOverrides* CompressionOverrides)
{
	// SoundWaveProcedural does not have compressed data and should generally not be asked about it
	return nullptr;
}

void UPicoAudioSoundWave::Serialize(FArchive& Ar)
{
	// Do not call the USoundWave version of serialize
	USoundBase::Serialize(Ar);
}

void UPicoAudioSoundWave::InitAudioResource(FByteBulkData& CompressedData)
{
	// Should never be pushing compressed data to a SoundWaveProcedural
	check(false);
}

bool UPicoAudioSoundWave::InitAudioResource(FName Format)
{
	// Nothing to be done to initialize a USoundWaveProcedural
	return true;
}

double UPicoAudioSoundWave::GetLastFedPCMTimestamp() const
{
	std::lock_guard lock(m_stats_mutex);

	return m_lastPCMGenerationFedTimestamp;
}

double UPicoAudioSoundWave::GetLastReceivedPCMTimestamp() const
{
	std::lock_guard lock(m_stats_mutex);

	return m_lastReceivedAudioTimestamp;
}

void UPicoAudioSoundWave::SetMissingFrameCounter(std::shared_ptr<EvercoastPerfCounter> counter)
{
	m_missingFrameCounter = counter;
	m_missingFrameCounter->Reset();
}

void UPicoAudioSoundWave::SetAudioBufferDelay(double delayInSeconds)
{
	std::lock_guard lock(m_mutex);

	m_audioBufferPumpDelay = delayInSeconds;
}

void UPicoAudioSoundWave::SetWarmupTime(float warmupTime)
{
	m_warmupTime = warmupTime;
}


float UPicoAudioSoundWave::GetCachedAudioTime() const
{
	std::lock_guard lock(m_mutex);

	if(!m_segments.empty())
	{
		// Assume every segments have the same duration
		return m_segments.front().duration * m_segments.size();
	}

	return 0.0f;
}


float UPicoAudioSoundWave::GetSecondaryCachedAudioTime() const
{
	std::lock_guard lock(m_mutex);

	return (float)m_audioBuffer.Num() / (float)(NumChannels * SampleRate * SampleByteSize);
}
