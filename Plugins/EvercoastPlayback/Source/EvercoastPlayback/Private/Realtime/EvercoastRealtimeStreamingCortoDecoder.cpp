#include "Realtime/EvercoastRealtimeStreamingCortoDecoder.h"
#include "Realtime/CortoImageUnifiedDecodeResult.h"
#include "CortoDecoder.h"
#include "WebpDecoder.h"
#include "Realtime/PicoQuicStreamingReaderComp.h"
#include "Realtime/RealtimeMeshingPacketHeader.h"
#include "EvercoastPerfCounter.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include <thread>
#include <chrono>
#include "Misc/Optional.h" // iOS hates C++17?
#include <mutex>

// An asynchronised decode thread doing flipflop between decoding and output
class RealtimeMeshImgSeqDecodeThread final : public FRunnable
{
public:
	struct InputFrame
	{
		double timestamp{ 0.0 };
		int64_t frameIndex{ 0 };
		TArray<uint8> data;
	};
	RealtimeMeshImgSeqDecodeThread(std::shared_ptr<CortoDecoder> meshDecoder, std::shared_ptr<WebpDecoder> imgDecoder, std::shared_ptr<EvercoastPerfCounter> perfCounter) :
		m_meshDecoder(meshDecoder), m_imgDecoder(imgDecoder), m_outputFlipflop(-1), m_perfCounter(perfCounter)
	{
	}

	~RealtimeMeshImgSeqDecodeThread()
	{
	}

	bool Init() override
	{
		for (int i = 0; i < 2; ++i)
		{
			m_output[i] = nullptr;
		}
		m_outputFlipflop = 0;
		m_running = true;
		return m_meshDecoder != nullptr;
	}

	uint32 Run() override
	{
		while (m_running)
		{
			TOptional<InputFrame> dataFrame;
			{
				std::lock_guard<std::mutex> guardInput(m_inputMutex);
				if (m_input.IsSet())
				{
					dataFrame = m_input;
					m_input.Reset();
				}
			}

			if (dataFrame.IsSet() && dataFrame->data.Num() > 0)
			{
				CortoDecodeOption meshDecodeOption;
				GenericDecodeOption imgDecodeOption;

				int current = (m_outputFlipflop + 1) % 2;
				std::lock_guard<std::mutex> guardOutput(m_outputMutex[current]);

				if (!m_output[current])
				{
					m_output[current] = std::make_shared<CortoImageUnifiedDecodeResult>(CortoDecoder::DEFAULT_VERTEX_COUNT, CortoDecoder::DEFAULT_TRIANGLE_COUNT, 1024, 1024, 32);
				}

				m_meshDecoder->SetReceivingResult(m_output[current]->meshResult);
				m_imgDecoder->SetReceivingResult(m_output[current]->imgResult);


				RealtimeMeshingPacketHeaderV1* header = (RealtimeMeshingPacketHeaderV1*)dataFrame->data.GetData();
				uint8_t* data = dataFrame->data.GetData();
				// find offset and data size and feed respectively
				uint8_t* cortoData = data + header->absoluteOffsetToCortoData;
				uint8_t* webpData = data + header->absoluteOffsetToWepPData;
				uint32_t cortoDataSize = header->cortoDataLength;
				uint32_t webpDataSize = header->wepPDataLength;

				// Guard from "empty frames"
				if (!cortoData || !webpData || cortoDataSize == 0 || webpDataSize == 0)
				{
					UE_LOG(EvercoastRealtimeLog, Warning, TEXT("Received empty frame(%d)! Mesh: %X(%d) Img: %X(%d)"), header->frameNumber, cortoData, cortoDataSize, webpData, webpDataSize);
				}
				else
				{
					if (m_meshDecoder->DecodeMemoryStream(cortoData, cortoDataSize, dataFrame->timestamp, dataFrame->frameIndex, &meshDecodeOption) &&
						m_imgDecoder->DecodeMemoryStream(webpData, webpDataSize, dataFrame->timestamp, dataFrame->frameIndex, &imgDecodeOption))
					{
						m_output[current]->SyncWithMeshResult();
						m_outputFlipflop = current;

						m_perfCounter->AddSample();
					}
					else
					{
						UE_LOG(EvercoastRealtimeLog, Warning, TEXT("Decode mesh/img failed"));
					}
				}

			}
			else
			{
				std::unique_lock<std::mutex> newFrameLock(m_newFrameMutex);
				m_condition.wait(newFrameLock);
			}

		}

		return 0;
	}

	void Stop() override
	{
		m_running = false;
		m_condition.notify_one();
	}

	void Exit() override
	{
		// nothing to do

	}


	bool AddEntry(double timestamp, int64_t frameIndex, const uint8_t* data, size_t dataSize)
	{
		TArray<uint8> InputBuffer;
		InputBuffer.AddUninitialized(dataSize);
		FMemory::Memcpy(InputBuffer.GetData(), data, dataSize);

		{
			std::lock_guard<std::mutex> guardInput(m_inputMutex);
			m_input = InputFrame{
				timestamp, frameIndex, std::move(InputBuffer)
			};
		}

		m_condition.notify_one();

		return true;
	}

	std::shared_ptr<CortoImageUnifiedDecodeResult> PopResult()
	{
		std::lock_guard<std::mutex> guardOutput(m_outputMutex[m_outputFlipflop]);

		if (m_output[m_outputFlipflop])
		{
			auto result = std::make_shared<CortoImageUnifiedDecodeResult>(*m_output[m_outputFlipflop]);
			m_output[m_outputFlipflop] = nullptr;

			return result;
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr<CortoImageUnifiedDecodeResult> PeekResult(int offsetFromTop) const
	{
		// don't have a queue, always return the current available
		std::lock_guard<std::mutex> guardOutput(m_outputMutex[m_outputFlipflop]);

		if (m_output[m_outputFlipflop])
		{
			return std::make_shared<CortoImageUnifiedDecodeResult>(*m_output[m_outputFlipflop]);
		}
		else
		{
			return nullptr;
		}
	}

	void Reset()
	{
		for (int i = 0; i < 2; ++i)
		{
			std::lock_guard<std::mutex> guardOutput(m_outputMutex[i]);
			if (m_output[i])
			{
				m_output[i]->InvalidateResult();
			}
			m_outputFlipflop = 0;
		}
	}


private:

	std::shared_ptr<CortoDecoder> m_meshDecoder;
	std::shared_ptr<WebpDecoder> m_imgDecoder;

	TOptional<InputFrame> m_input;
	std::shared_ptr<CortoImageUnifiedDecodeResult> m_output[2];
	mutable std::mutex m_outputMutex[2];
	int m_outputFlipflop;

	std::atomic<bool> m_running{ false };

	std::mutex m_inputMutex;

	std::mutex m_newFrameMutex;
	std::condition_variable m_condition;
	std::shared_ptr<EvercoastPerfCounter> m_perfCounter;
};


EvercoastRealtimeStreamingCortoDecoder::EvercoastRealtimeStreamingCortoDecoder(std::shared_ptr<EvercoastPerfCounter> perfCounter) :
	m_baseMeshDecoder(CortoDecoder::Create()), m_baseImageDecoder(WebpDecoder::Create()), m_runnable(nullptr), m_runnableController(nullptr)
{
	m_runnable = new RealtimeMeshImgSeqDecodeThread(m_baseMeshDecoder, m_baseImageDecoder, perfCounter);
	m_runnableController = FRunnableThread::Create(m_runnable, TEXT("Evercoast Realtime Mesh Decode Thread"));
}


EvercoastRealtimeStreamingCortoDecoder::~EvercoastRealtimeStreamingCortoDecoder()
{
	m_runnableController->Kill(true);
	delete m_runnableController;
	delete m_runnable;
}


void EvercoastRealtimeStreamingCortoDecoder::Receive(double timestamp, int64_t frameIndex, const uint8_t* data, size_t data_size, uint32_t metadata)
{
	RealtimeMeshImgSeqDecodeThread* decodeThread = static_cast<RealtimeMeshImgSeqDecodeThread*>(m_runnable);
	if (!decodeThread->AddEntry(timestamp, frameIndex, data, data_size))
	{
		//UE_LOG(EvercoastDecoderLog, Warning, TEXT("Add entry to decoder failed. Buffer is full."));
	}
}

std::shared_ptr<GenericDecodeResult> EvercoastRealtimeStreamingCortoDecoder::PopResult()
{
	RealtimeMeshImgSeqDecodeThread* decodeThread = static_cast<RealtimeMeshImgSeqDecodeThread*>(m_runnable);
	return decodeThread->PopResult();
}

std::shared_ptr<GenericDecodeResult> EvercoastRealtimeStreamingCortoDecoder::PeekResult(int offsetFromTop) const
{
	RealtimeMeshImgSeqDecodeThread* decodeThread = static_cast<RealtimeMeshImgSeqDecodeThread*>(m_runnable);
	return decodeThread->PeekResult(offsetFromTop);
}

void EvercoastRealtimeStreamingCortoDecoder::DisposeResult(std::shared_ptr<GenericDecodeResult> result)
{
	// no need to dispose, everything is copied out
}

void EvercoastRealtimeStreamingCortoDecoder::FlushAndDisposeResults()
{
	RealtimeMeshImgSeqDecodeThread* decodeThread = static_cast<RealtimeMeshImgSeqDecodeThread*>(m_runnable);
	decodeThread->Reset();
}
