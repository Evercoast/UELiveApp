#include "RuntimeAudioFactory.h"
#include "EvercoastHeader.h"
#include "RuntimeAudio.h"
#include "WavDecoder.h"
#include "Mp3Decoder.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"


URuntimeAudio* FRuntimeAudioFactory::CreateRuntimeAudioFromBufferSync(const TArray<uint8>& buffer)
{
	// The reason behind the casting hack is we simply need to fix the duration field in some wav files
	if (WavDecoder::ValidateAndFixDuration(const_cast<uint8*>(buffer.GetData()), buffer.Num()))
	{
		RuntimeAudioData audioData;
		if (WavDecoder::Decode(buffer.GetData(), buffer.Num(), &audioData.Metadata, &audioData.PCMData) &&
			!IsGarbageCollecting())
		{
			auto runtimeAudio = NewObject<URuntimeAudio>();
			runtimeAudio->UpdateData(audioData);
			return runtimeAudio;
		}
	}
	else if (Mp3Decoder::Validate(buffer.GetData(), buffer.Num()))
	{
		RuntimeAudioData audioData;
		if (Mp3Decoder::Decode(buffer.GetData(), buffer.Num(), &audioData.Metadata, &audioData.PCMData) &&
			!IsGarbageCollecting())
		{
			auto runtimeAudio = NewObject<URuntimeAudio>();
			runtimeAudio->UpdateData(audioData);
			return runtimeAudio;
		}
	}

	return nullptr;
}


class FAudioDecodeThread : public FRunnable
{
	TArray<uint8_t> m_encodedData;
	RuntimeAudioData m_audioData;
	FOnRuntimeAudioFactoryResult m_callback;
public:
	FAudioDecodeThread(const uint8_t* pData, int32_t dataSize, FOnRuntimeAudioFactoryResult callback)
	{
		m_encodedData.Append(pData, dataSize);
		m_callback = callback;
	}

	bool Init() override
	{
		if (!WavDecoder::ValidateAndFixDuration(m_encodedData.GetData(), m_encodedData.Num()))
		{
			if (m_callback.IsBound())
			{
				m_callback.Broadcast(nullptr, ERuntimeAudioFactoryResult::Failed_Decoding);
			}
			return false;
		}
		return true;
	}

	uint32 Run() override
	{
		if (WavDecoder::Decode(m_encodedData.GetData(), m_encodedData.Num(), &m_audioData.Metadata, &m_audioData.PCMData))
		{
			auto runtimeAudio = NewObject<URuntimeAudio>();
			if (!runtimeAudio || IsGarbageCollecting())
			{
				if (m_callback.IsBound())
				{
					m_callback.Broadcast(runtimeAudio, ERuntimeAudioFactoryResult::Failed_CreatingAudioObject);
					return -1;
				}
			}

			runtimeAudio->UpdateData(m_audioData);

			if (m_callback.IsBound())
			{
				m_callback.Broadcast(runtimeAudio, ERuntimeAudioFactoryResult::Succeeded);
			}
			return 0;
		}

		if (m_callback.IsBound())
		{
			m_callback.Broadcast(nullptr, ERuntimeAudioFactoryResult::Failed_Decoding);
		}
		return -1;
	}

	void Stop() override
	{
	}

	void Exit() override
	{
		m_encodedData.Empty();
		m_audioData.PCMData.Empty();
	}
};

static int s_AudioThreadCounter = 0;

std::shared_ptr<FRunnableThread> FRuntimeAudioFactory::CreateRuntimeAudioFromBuffer(const TArray<uint8>& buffer, FOnRuntimeAudioFactoryResult callback)
{
	FOnRuntimeAudioFactoryResult gameThreadCallback;
	gameThreadCallback.AddLambda([callback](URuntimeAudio* audio, ERuntimeAudioFactoryResult result) {
		AsyncTask(ENamedThreads::GameThread, [callback, audio, result]()
			{
				if (callback.IsBound())
				{
					callback.Broadcast(audio, result);
				}
			});
	});
	FAudioDecodeThread* runnable = new FAudioDecodeThread(buffer.GetData(), buffer.Num(), gameThreadCallback);
	FRunnableThread* runnableThread = FRunnableThread::Create(runnable, *FString::Printf( TEXT("Evercoast Audio Decode Thread %d"), s_AudioThreadCounter++));
	return std::shared_ptr<FRunnableThread>(runnableThread);
}