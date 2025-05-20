#include "Realtime/EvercoastRealtimeStreamingVoxelDecoder.h"
#include <mutex>
#include "EvercoastVoxelDecoder.h"
#include "Realtime/PicoQuicStreamingReaderComp.h"
#include "EvercoastPerfCounter.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include <thread>
#include <chrono>
#include "Misc/Optional.h" // iOS hates C++17?

// An asynchronised decode thread but no control over what timestamp the frame it can get
class RealtimeVoxelDecodeThread final : public FRunnable
{
public:
	struct InputFrame
	{
		double timestamp{ 0.0 };
		int64_t frameIndex{ 0 };
		TArray<uint8> data;
	};
	RealtimeVoxelDecodeThread(std::shared_ptr<IGenericDecoder> decoder, std::shared_ptr<EvercoastPerfCounter> perfCounter) :
		m_baseDecoder(decoder), m_perfCounter(perfCounter)
	{
		auto voxelDecoder = std::static_pointer_cast<EvercoastVoxelDecoder>(m_baseDecoder);
		m_baseDefinition = voxelDecoder->GetDefaultDefinition();
		m_baseDefinition.fast_decode = true;
		m_baseDefinition.rgb_colourspace_conversion = false;
		m_baseDefinition.cartesian_space_conversion = true;
		m_baseDefinition.gfx_api_compatibility_mode = true;
		m_baseDefinition.half_float_coordinates = false;
	}

	~RealtimeVoxelDecodeThread()
	{
	}

	bool Init() override
	{
		m_running = true;
		return m_baseDecoder != nullptr;
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
					dataFrame =  m_input;
					m_input.Reset();
				}
			}

			if (dataFrame.IsSet() && dataFrame->data.Num() > 0)
			{
				EvercoastVoxelDecodeOption option(m_baseDefinition);
				if (m_baseDecoder->DecodeMemoryStream(dataFrame->data.GetData(), dataFrame->data.Num(), dataFrame->timestamp, dataFrame->frameIndex, &option))
				{
					auto result = m_baseDecoder->TakeResult();
					auto pResult = std::static_pointer_cast<EvercoastVoxelDecodeResult>(result);
					check(pResult->resultFrame);

					{
						std::lock_guard<std::mutex> guardOutput(m_outputMutex);
						m_output = pResult;

						m_perfCounter->AddSample();
					}
				}
				else
				{
					UE_LOG(EvercoastRealtimeLog, Warning, TEXT("Decode voxel failed"));
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

	std::shared_ptr<EvercoastVoxelDecodeResult> PopResult()
	{
		std::lock_guard<std::mutex> guardOutput(m_outputMutex);
		if (m_output)
		{
			return std::move(m_output);
		}
		else
		{
			return std::make_shared<EvercoastVoxelDecodeResult>(false, -1.0, -1, InvalidHandle);
		}
	}

	std::shared_ptr<EvercoastVoxelDecodeResult> PeekResult(int offsetFromTop) const
	{
		return std::make_shared<EvercoastVoxelDecodeResult>(false, -1.0, -1, InvalidHandle);
	}


private:

	std::shared_ptr<IGenericDecoder> m_baseDecoder;
	Definition m_baseDefinition;

	TOptional<InputFrame> m_input;
	std::shared_ptr<EvercoastVoxelDecodeResult> m_output;

	std::atomic<bool> m_running{ false };

	std::mutex m_inputMutex;
	std::mutex m_outputMutex;

	std::mutex m_newFrameMutex;
	std::condition_variable m_condition;

	std::shared_ptr<EvercoastPerfCounter> m_perfCounter;
};

EvercoastRealtimeStreamingVoxelDecoder::EvercoastRealtimeStreamingVoxelDecoder(std::shared_ptr<EvercoastPerfCounter> perfCounter) :
	m_baseVoxelDecoder(EvercoastVoxelDecoder::Create()), m_runnable(nullptr), m_runnableController(nullptr)
{
	m_runnable = new RealtimeVoxelDecodeThread(m_baseVoxelDecoder, perfCounter);
	m_runnableController = FRunnableThread::Create(m_runnable, TEXT("Evercoast Realtime Voxel Decode Thread"));
}

EvercoastRealtimeStreamingVoxelDecoder::~EvercoastRealtimeStreamingVoxelDecoder()
{
	m_runnableController->Kill(true);
	delete m_runnableController;
	delete m_runnable;
}

void EvercoastRealtimeStreamingVoxelDecoder::Receive(double timestamp, int64_t frameIndex, const uint8_t* data, size_t data_size, uint32_t metadata)
{
	RealtimeVoxelDecodeThread* decodeThread = static_cast<RealtimeVoxelDecodeThread*>(m_runnable);
	if (!decodeThread->AddEntry(timestamp, frameIndex, data, data_size))
	{
		//UE_LOG(EvercoastVoxelDecoderLog, Warning, TEXT("Add entry to decoder failed. Buffer is full."));
	}
}

std::shared_ptr<GenericDecodeResult> EvercoastRealtimeStreamingVoxelDecoder::PopResult()
{
	RealtimeVoxelDecodeThread* decodeThread = static_cast<RealtimeVoxelDecodeThread*>(m_runnable);
	return decodeThread->PopResult();
}

std::shared_ptr<GenericDecodeResult> EvercoastRealtimeStreamingVoxelDecoder::PeekResult(int offsetFromTop) const
{
	RealtimeVoxelDecodeThread* decodeThread = static_cast<RealtimeVoxelDecodeThread*>(m_runnable);
	return decodeThread->PeekResult(offsetFromTop);
}

void EvercoastRealtimeStreamingVoxelDecoder::DisposeResult(std::shared_ptr<GenericDecodeResult> result)
{
	result->InvalidateResult();
}

void EvercoastRealtimeStreamingVoxelDecoder::FlushAndDisposeResults()
{
	RealtimeVoxelDecodeThread* decodeThread = static_cast<RealtimeVoxelDecodeThread*>(m_runnable);
	auto result = decodeThread->PopResult();
	DisposeResult(result);
}
