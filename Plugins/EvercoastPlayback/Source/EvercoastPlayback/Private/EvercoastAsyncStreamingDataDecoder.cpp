#include "EvercoastAsyncStreamingDataDecoder.h"
#include <mutex>
#include "EvercoastDecoder.h"
#include "EvercoastLocalDataFrame.h"
#include "GhostTreeFormatReader.h"
#include "CortoDecoder.h"
#include "WebpDecoder.h"
#include "CortoWebpUnifiedDecodeResult.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include <thread>
#include <chrono>

// An asynchronised decode thread but no control over what timestamp the frame it can get
class VoxelDecodeThread final : public FRunnable
{
public:
	VoxelDecodeThread(std::shared_ptr<IGenericDecoder> decoder, int bufferCount) :
		m_baseDecoder(decoder), m_bufferCount(bufferCount), m_running(false)
	{
		auto voxelDecoder = std::static_pointer_cast<EvercoastDecoder>(m_baseDecoder);
		m_baseDefinition = voxelDecoder->GetDefaultDefinition();
		m_baseDefinition.fast_decode = true;
		m_baseDefinition.rgb_colourspace_conversion = false;
		m_baseDefinition.cartesian_space_conversion = true;
		m_baseDefinition.gfx_api_compatibility_mode = true;
		m_baseDefinition.half_float_coordinates = false;

		m_localDataFrameList = new std::shared_ptr<EvercoastLocalDataFrame>[m_bufferCount];
		m_outputFrameList = new std::shared_ptr<EvercoastDecodeResult>[m_bufferCount];
	}

	void ResizeBuffer(uint32_t bufferSize)
	{
		delete[] m_localDataFrameList;
		delete[] m_outputFrameList;

		m_bufferCount = bufferSize;

		m_localDataFrameList = new std::shared_ptr<EvercoastLocalDataFrame>[m_bufferCount];
		m_outputFrameList = new std::shared_ptr<EvercoastDecodeResult>[m_bufferCount];
	}

	~VoxelDecodeThread()
	{
		delete[] m_localDataFrameList;
		delete[] m_outputFrameList;
	}

	bool Init() override
	{
		m_currInputWriteIdx = -1;
		m_currInputReadIdx = -1;
		m_currIOIdx = -1;
		m_currOutputReadIdx = -1;
		m_running = true;
		return m_baseDecoder != nullptr;
	}

	uint32 Run() override
	{
		while (true)
		{
			{
                std::lock_guard<std::recursive_mutex> guardControl(m_controllerMutex);
                if (!m_running)
                    break;
            }

			if (HasNewEntry())
			{
				std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);

				UE_LOG(EvercoastDecoderLog, Verbose, TEXT("Decode: Input Read: %d"), m_currInputReadIdx);
				std::shared_ptr<EvercoastLocalDataFrame>& dataFrame = m_localDataFrameList[NextInputReadIndex_NTS()];

				EvercoastDecodeOption option(m_baseDefinition);
				if (m_baseDecoder->DecodeMemoryStream(dataFrame->m_data, dataFrame->m_dataSize, dataFrame->m_timestamp, dataFrame->m_frameIndex, &option))
				{
					auto result = m_baseDecoder->GetResult();
					auto pResult = std::static_pointer_cast<EvercoastDecodeResult>(result);
					check(pResult->resultFrame);

					std::lock_guard<std::recursive_mutex> guardOutput(m_outputMutex);
					m_outputFrameList[NextIOIndex_NTS()] = pResult;
				}
				else
				{
					UE_LOG(EvercoastDecoderLog, Warning, TEXT("Decode voxel failed"));
				}

			}
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(5ms);


		}

		return 0;
	}

	void Stop() override
	{
		std::lock_guard<std::recursive_mutex> guard(m_controllerMutex);
		m_running = false;
	}

	void Exit() override
	{
		// nothing to do

	}

	bool HasNewEntry() const
	{
		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);

		return m_currInputWriteIdx != m_currInputReadIdx;
	}

	bool AddEntry(double timestamp, int64_t frameIndex, const uint8_t* data, size_t dataSize)
	{
		if (IsBufferFull())
			return false;

		UE_LOG(EvercoastDecoderLog, Verbose, TEXT("AddEntry: %.5f Input Write: %d"), timestamp, m_currInputWriteIdx);

		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);
		auto localDataFrame = std::make_shared<EvercoastLocalDataFrame>(timestamp, frameIndex, data, dataSize);
		m_localDataFrameList[NextInputWriteIndex_NTS()] = localDataFrame;



		return true;
	}

	std::shared_ptr<EvercoastDecodeResult> PopResult()
	{
		std::lock_guard<std::recursive_mutex> guardOutput(m_outputMutex);
		if (!HasDecodingStarted())
			return std::make_shared<EvercoastDecodeResult>(false, -1.0, -1, InvalidHandle);
		if (IsOutputBufferUnderrun())
			return std::make_shared<EvercoastDecodeResult>(false, -1.0, -1, InvalidHandle);

		UE_LOG(EvercoastDecoderLog, Verbose, TEXT("PopResult: IO: %d, Output Read: %d"), m_currIOIdx, m_currOutputReadIdx);

		int readBuffer = NextOutputReadIndex_NTS();

		return m_outputFrameList[readBuffer];
	}

	std::shared_ptr<EvercoastDecodeResult> PeekResult(int offsetFromTop) const
	{
		std::lock_guard<std::recursive_mutex> guardOutput(m_outputMutex);
		if (!HasDecodingStarted())
			return std::make_shared<EvercoastDecodeResult>(false, -1.0, -1, InvalidHandle);
		if (IsOutputBufferUnderrun(offsetFromTop))
			return std::make_shared<EvercoastDecodeResult>(false, -1.0, -1, InvalidHandle);

		UE_LOG(EvercoastDecoderLog, Verbose, TEXT("PeekResult: IO: %d, Output Read: %d"), m_currIOIdx, m_currOutputReadIdx);

		int readBuffer = GetNextOutputReadIndex_NTS(offsetFromTop);

		return m_outputFrameList[readBuffer];
	}


private:

	int NextInputReadIndex_NTS()
	{
		m_currInputReadIdx = (m_currInputReadIdx + 1) % m_bufferCount;
		return m_currInputReadIdx;
	}

	int NextInputWriteIndex_NTS()
	{
		m_currInputWriteIdx = (m_currInputWriteIdx + 1) % m_bufferCount;
		return m_currInputWriteIdx;
	}

	int NextIOIndex_NTS()
	{
		m_currIOIdx = (m_currIOIdx + 1) % m_bufferCount;
		return m_currIOIdx;
	}

	int GetNextIOIndex_NTS() const
	{
		return(m_currIOIdx + 1) % m_bufferCount;
	}

	int NextOutputReadIndex_NTS()
	{
		m_currOutputReadIdx = (m_currOutputReadIdx + 1) % m_bufferCount;
		return m_currOutputReadIdx;
	}

	int GetNextOutputReadIndex_NTS(int offsetFromTop) const
	{
		return (m_currOutputReadIdx + 1 + offsetFromTop) % m_bufferCount;
	}

	bool IsBufferFull() const
	{
		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);

		if (m_currInputWriteIdx < 0 || m_currOutputReadIdx < 0)
			return false;

		std::lock_guard<std::recursive_mutex> guardOutput(m_outputMutex);
		return (m_currInputWriteIdx + 1) % m_bufferCount == m_currOutputReadIdx;
	}

	bool IsOutputBufferUnderrun(int offsetFromTop = 0) const
	{
		std::lock_guard<std::recursive_mutex> guard(m_outputMutex);
		if (m_currOutputReadIdx < 0 || m_currIOIdx < 0)
			return false;
		return (m_currOutputReadIdx + offsetFromTop) == m_currIOIdx;
	}

	// Merge this with IsOutputBufferUnderrun()?
	bool HasDecodingStarted() const
	{
		std::lock_guard<std::recursive_mutex> guardOutput(m_outputMutex);
		if (m_currIOIdx >= 0)
			return true;
		return false;
	}

	std::shared_ptr<IGenericDecoder> m_baseDecoder;
	Definition m_baseDefinition;

	std::shared_ptr<EvercoastLocalDataFrame>* m_localDataFrameList;
	std::shared_ptr<EvercoastDecodeResult>* m_outputFrameList; // array of shared_ptrs
	int m_bufferCount;
	int m_currInputWriteIdx;
	int m_currInputReadIdx;
	int m_currIOIdx;
	int m_currOutputReadIdx;
	bool m_running;

	mutable std::recursive_mutex m_controllerMutex;
	mutable std::recursive_mutex m_inputMutex;
	mutable std::recursive_mutex m_outputMutex;
};


class CortoDecodeThread final : public FRunnable
{
	
public:
	CortoDecodeThread(std::shared_ptr<IGenericDecoder> mesh_decoder, std::shared_ptr<IGenericDecoder> image_decoder, int bufferCount) :
		m_cortoDecoder(std::static_pointer_cast<CortoDecoder>(mesh_decoder)), 
		m_webpDecoder(std::static_pointer_cast<WebpDecoder>(image_decoder)), m_bufferCount(bufferCount), m_running(false), m_requiresExternalData(false)
	{
		m_localDataFrameList.resize(m_bufferCount);
		m_outputFrameList.resize(m_bufferCount);

		for (int i = 0; i < m_bufferCount; ++i)
		{
			m_outputFrameList[i] = std::make_shared<CortoWebpUnifiedDecodeResult>(CortoDecoder::DEFAULT_VERTEX_COUNT, CortoDecoder::DEFAULT_TRIANGLE_COUNT, 1024, 1024, 32);
		}
	}

	~CortoDecodeThread()
	{
		m_localDataFrameList.clear();
		m_outputFrameList.clear();
		m_cortoDecoder.reset();
		m_webpDecoder.reset();
	}

	bool Init() override
	{
		m_currInputWriteIdx = -1;
		m_currInputReadIdx = -1;
		m_currIOIdx = -1;
		m_currOutputReadIdx = -1;
		m_running = true;
		return m_cortoDecoder != nullptr;
	}

	void ResizeBuffer(uint32_t bufferSize)
	{
		m_bufferCount = bufferSize;

		m_localDataFrameList.resize(m_bufferCount);
		m_outputFrameList.resize(m_bufferCount);

		for (int i = 0; i < m_bufferCount; ++i)
		{
			m_outputFrameList[i] = std::make_shared<CortoWebpUnifiedDecodeResult>(CortoDecoder::DEFAULT_VERTEX_COUNT, CortoDecoder::DEFAULT_TRIANGLE_COUNT, 1024, 1024, 32);
		}
	}

	void SetRequiresExternalData(bool required)
	{
		m_requiresExternalData = required;
	}

	uint32 Run() override
	{
#if ENGINE_MAJOR_VERSION == 5
		FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
#endif
		while (true)
		{
            {
                std::lock_guard<std::recursive_mutex> guardControl(m_controllerMutex);
                if (!m_running)
                    break;
            }

			if (HasNewEntry())
			{
				int lastDecodedWidth = -1;
				int lastDecodedHeight = -1;

				std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);

				
				int nextInputReadIdx = GetNextInputReadIndex_NTS();
				UE_LOG(EvercoastDecoderLog, VeryVerbose, TEXT("Decode: Input Read: %d"), nextInputReadIdx);

				std::shared_ptr<ECMLocalDataFrame>& dataFrame = m_localDataFrameList[nextInputReadIdx];
				CortoDecodeOption option;

				if (dataFrame->IsReady())
				{
					// Only advance input read index when it's ready
					NextInputReadIndex_NTS();

					// protect output array
					std::lock_guard<std::recursive_mutex> guardOutput(m_outputMutex);
					auto nextIOIndex = GetNextIOIndex_NTS();
                    
                    std::lock_guard<std::mutex> outputCreationLock(m_textureCreationMutex);
					auto unifiedResult = m_outputFrameList[nextIOIndex];
					m_cortoDecoder->SetReceivingResult(m_outputFrameList[nextIOIndex]->meshResult);
					if (m_webpDecoder)
						m_webpDecoder->SetReceivingResult(m_outputFrameList[nextIOIndex]->imgResult);

					bool requireImageDecoding = !m_requiresExternalData;
					if (requireImageDecoding)
					{
						

						if (m_cortoDecoder->DecodeMemoryStream(dataFrame->m_data, dataFrame->m_dataSize, dataFrame->m_timestamp, dataFrame->m_frameIndex, &option) &&
							m_webpDecoder->DecodeMemoryStream(dataFrame->m_imageData, dataFrame->m_imageDataSize, dataFrame->m_timestamp, dataFrame->m_frameIndex, &option))
						{
							unifiedResult->SyncWithMeshResult();
							
							// just bump the io index, no need to get result explicitly from decoder, as it was set as receiver in the above line.
							NextIOIndex_NTS();
							
						}
						else
						{
							UE_LOG(EvercoastDecoderLog, Warning, TEXT("Decode mesh+image failed"));
						}
					}
					else
					{
						if (m_cortoDecoder->DecodeMemoryStream(dataFrame->m_data, dataFrame->m_dataSize, dataFrame->m_timestamp, dataFrame->m_frameIndex, &option))
						{
							unifiedResult->SyncWithMeshResult();
							// just bump the io index, no need to get result explicitly from decoder, as it was set as receiver in the above line.
							NextIOIndex_NTS();

						}
						else
						{
							UE_LOG(EvercoastDecoderLog, Warning, TEXT("Decode mesh failed"));
						}
					}

					dataFrame->Invalidate();
					//dataFrame.reset();
				}

			}

			using namespace std::chrono_literals;
			std::this_thread::sleep_for(5ms);


		}

		return 0;
	}

	void Stop() override
	{
#if ENGINE_MAJOR_VERSION == 5
		FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
#endif
		{
			std::lock_guard<std::recursive_mutex> guard(m_controllerMutex);
			m_running = false;
		}

		// reset control variables too, just in case while waiting for thread to finish, the loop tries to get more item from the queue
		{
			std::lock_guard<std::recursive_mutex> guard(m_inputMutex);
			m_currInputWriteIdx = -1;
			m_currInputReadIdx = -1;
		}

		{
			std::lock_guard<std::recursive_mutex> guard(m_outputMutex);
			m_currIOIdx = -1;
			m_currOutputReadIdx = -1;
		}

	}

	void Exit() override
	{
#if ENGINE_MAJOR_VERSION == 5
		FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
#endif
		m_localDataFrameList.clear();
		m_outputFrameList.clear();

		m_cortoDecoder.reset();
		m_webpDecoder.reset();

	}

	void _PrintBufferContents()
	{
		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);
		for (int i = 0; i < m_bufferCount; ++i)
		{
			UE_LOG(EvercoastDecoderLog, Verbose, TEXT("Full input_buffer[%d] = %lld mesh ready: %d img ready: %d"), i, m_localDataFrameList[i]->m_frameIndex, m_localDataFrameList[i]->m_data != nullptr, m_localDataFrameList[i]->m_imageData != nullptr);
		}
	}

	bool HasNewEntry() const
	{
		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);

		return m_currInputWriteIdx != m_currInputReadIdx;
	}

	int FindLocalDataByFrameIndex(int64_t frameIndex)
	{
		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);
		for (int i = 0; i < m_bufferCount; ++i)
		{
			if (m_localDataFrameList[i] && m_localDataFrameList[i]->m_frameIndex == frameIndex)
				return i;
		}

		return -1;
	}

	bool AddEntry(double timestamp, int64_t frameIndex, const uint8_t* data, size_t dataSize, uint32_t metadata)
	{
#if ENGINE_MAJOR_VERSION == 5
		FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
#endif
		bool isImage = (metadata == UGhostTreeFormatReader::DECODE_META_IMAGE_WEBP);

		// Check if we need to update existing input frames first
		{
			std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);
			int existingFrameIndex = FindLocalDataByFrameIndex(frameIndex);
			if (existingFrameIndex >= 0)
			{
				m_localDataFrameList[existingFrameIndex]->UpdateData(data, dataSize, isImage);
				UE_LOG(EvercoastDecoderLog, Verbose, TEXT("AddEntry(update) input_buffer[%d] = %lld %s"), existingFrameIndex, frameIndex, isImage ? TEXT("image_data") : TEXT("mesh_data"));
				return true;
			}
		}

		// Not updating, then check if input buffer is full
		if (IsBufferFull())
		{
			if (m_requiresExternalData)
				return false;

			UE_LOG(EvercoastDecoderLog, Verbose, TEXT("Try AddEntry but full: %lld %s"), frameIndex, isImage ? TEXT("image_data") : TEXT("mesh_data"));

            int newBufferCount = m_bufferCount * 2;
#if PLATFORM_IOS
            // iOS won't be able to handle too much buffered textures, maybe we should consider
            // delaying the texture creation to only when texture get popped from the result queue.
            if (newBufferCount >= 128)
                return false;
#endif
			// We have image data to dealt with, so it's special
			std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);
			std::lock_guard<std::recursive_mutex> guardOutput(m_outputMutex);
			// double the buffer!

            {
                std::lock_guard<std::mutex> lock(m_textureCreationMutex);
                
                m_localDataFrameList.resize(newBufferCount);
                m_outputFrameList.resize(newBufferCount);

                for (int i = m_bufferCount; i < newBufferCount; ++i)
                {
                    m_localDataFrameList[i] = std::make_shared<ECMLocalDataFrame>(-1, -1, nullptr, 0, !m_requiresExternalData, false);
                    m_outputFrameList[i] = nullptr;
                }
            }
            
            AsyncTask(ENamedThreads::GameThread, [self=this, oldBufferCount = m_bufferCount, newBufferCount] {
                
                std::lock_guard<std::mutex> lock(self->m_textureCreationMutex);
                for (int i = oldBufferCount; i < newBufferCount; ++i)
                {
                    self->m_outputFrameList[i] = std::make_shared<CortoWebpUnifiedDecodeResult>(CortoDecoder::DEFAULT_VERTEX_COUNT, CortoDecoder::DEFAULT_TRIANGLE_COUNT, 1024, 1024, 32);
                }
            }
            );

			m_bufferCount = newBufferCount;

			// reset output indices so that buffer won't appear as full
			m_currIOIdx = m_currInputWriteIdx;// -1;
			m_currOutputReadIdx = m_currInputWriteIdx;// -1;
			

			// Check if full again, if still full we have no choice but bail out
			if (IsBufferFull())
				return false;
		}

		auto localDataFrame = std::make_shared<ECMLocalDataFrame>(timestamp, frameIndex, data, dataSize, !m_requiresExternalData, isImage);
		m_localDataFrameList[NextInputWriteIndex_NTS()] = localDataFrame;

		UE_LOG(EvercoastDecoderLog, Verbose, TEXT("AddEntry(new) input_buffer[%d] = %lld %s"), m_currInputWriteIdx, frameIndex, isImage ? TEXT("image_data") : TEXT("mesh_data"));
		return true;
	}

	std::shared_ptr<GenericDecodeResult> PopResult()
	{
		std::lock_guard<std::recursive_mutex> guardOutput(m_outputMutex);
        std::lock_guard<std::mutex> lock(m_textureCreationMutex);
        
		if (!HasDecodingStarted())
			return std::make_shared<GenericDecodeResult>(false, 0, -1);
		if (IsOutputBufferUnderrun())
			return std::make_shared<GenericDecodeResult>(false, 0, -1);

		UE_LOG(EvercoastDecoderLog, Verbose, TEXT("PopResult: IO: %d, Output Read: %d"), m_currIOIdx, m_currOutputReadIdx);

		int readBuffer = NextOutputReadIndex_NTS();

		return m_outputFrameList[readBuffer];
	}

	std::shared_ptr<GenericDecodeResult> PeekResult(int offsetFromTop) const
	{
		std::lock_guard<std::recursive_mutex> guardOutput(m_outputMutex);
        std::lock_guard<std::mutex> lock(m_textureCreationMutex);
        
		if (!HasDecodingStarted())
			return std::make_shared<GenericDecodeResult>(false, 0, -1);
		if (IsOutputBufferUnderrun(offsetFromTop))
			return std::make_shared<GenericDecodeResult>(false, 0, -1);

		UE_LOG(EvercoastDecoderLog, Verbose, TEXT("PeekResult: IO: %d, Output Read: %d"), m_currIOIdx, m_currOutputReadIdx);

		int readBuffer = GetNextOutputReadIndex_NTS(offsetFromTop);
		return m_outputFrameList[readBuffer];
	}

	void InvalidateAllResults()
	{
		std::lock_guard<std::recursive_mutex> guardOutput(m_outputMutex);
        std::lock_guard<std::mutex> lock(m_textureCreationMutex);
        
		for (int i = 0; i < m_bufferCount; ++i)
		{
			m_outputFrameList[i]->InvalidateResult();
		}

	}

	void FlushAndDisposeResults()
	{
		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);

		Init();
	}

private:

	int NextInputReadIndex_NTS()
	{
		m_currInputReadIdx = (m_currInputReadIdx + 1) % m_bufferCount;
		return m_currInputReadIdx;
	}

	int GetNextInputReadIndex_NTS() const
	{
		return (m_currInputReadIdx + 1) % m_bufferCount;
	}

	int NextInputWriteIndex_NTS()
	{
		m_currInputWriteIdx = (m_currInputWriteIdx + 1) % m_bufferCount;
		return m_currInputWriteIdx;
	}

	int NextIOIndex_NTS()
	{
		m_currIOIdx = (m_currIOIdx + 1) % m_bufferCount;
		return m_currIOIdx;
	}

	int GetNextIOIndex_NTS() const
	{
		return (m_currIOIdx + 1) % m_bufferCount;
	}

	int NextOutputReadIndex_NTS()
	{
		m_currOutputReadIdx = (m_currOutputReadIdx + 1) % m_bufferCount;
		return m_currOutputReadIdx;
	}

	int GetNextOutputReadIndex_NTS(int offsetFromTop = 0) const
	{
		return (m_currOutputReadIdx + 1 + offsetFromTop) % m_bufferCount;
	}

	bool IsBufferFull() const
	{
		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);

		if (m_currInputWriteIdx < 0 || m_currOutputReadIdx < 0)
			return false;

		std::lock_guard<std::recursive_mutex> guardOutput(m_outputMutex);
		return (m_currInputWriteIdx + 1) % m_bufferCount == m_currOutputReadIdx;
	}

	bool IsOutputBufferUnderrun(int offsetFromTop = 0) const
	{
		std::lock_guard<std::recursive_mutex> guard(m_outputMutex);
		if (m_currOutputReadIdx < 0 || m_currIOIdx < 0)
			return false;
		return (m_currOutputReadIdx + offsetFromTop) == m_currIOIdx;
	}

	// Merge this with IsOutputBufferUnderrun()?
	bool HasDecodingStarted() const
	{
		std::lock_guard<std::recursive_mutex> guardOutput(m_outputMutex);
		if (m_currIOIdx >= 0)
			return true;
		return false;
	}

	std::shared_ptr<CortoDecoder> m_cortoDecoder;
	std::shared_ptr<WebpDecoder> m_webpDecoder;

	std::vector<std::shared_ptr<ECMLocalDataFrame>> m_localDataFrameList;
	std::vector<std::shared_ptr<CortoWebpUnifiedDecodeResult>> m_outputFrameList; // a memory buffer storing shared_ptr<UnifiedDecodeResult>

	int m_bufferCount;
	int m_currInputWriteIdx;
	int m_currInputReadIdx;
	int m_currIOIdx;
	int m_currOutputReadIdx;
	bool m_running;
	bool m_requiresExternalData;

	mutable std::recursive_mutex m_controllerMutex;
	mutable std::recursive_mutex m_inputMutex;
	mutable std::recursive_mutex m_outputMutex;
    
    mutable std::mutex m_textureCreationMutex;
};


EvercoastAsyncStreamingDataDecoder::EvercoastAsyncStreamingDataDecoder(std::shared_ptr<IGenericDecoder> base_decoder, std::shared_ptr<IGenericDecoder> aux_decoder) :
	m_baseDecoder(base_decoder), m_auxDecoder(aux_decoder), m_runnable(nullptr), m_runnableController(nullptr)
{
	if (m_baseDecoder->GetType() == DT_EvercoastVoxel)
	{
		m_runnable = new VoxelDecodeThread(m_baseDecoder, DEFAULT_BUFFER_COUNT);
	}
	else
	{
		m_runnable = new CortoDecodeThread(m_baseDecoder, m_auxDecoder, DEFAULT_BUFFER_COUNT);
	}
	m_runnableController = FRunnableThread::Create(m_runnable, TEXT("Evercoast Decode Thread"));
}

EvercoastAsyncStreamingDataDecoder::~EvercoastAsyncStreamingDataDecoder()
{
	Deinit();
}

void EvercoastAsyncStreamingDataDecoder::Deinit()
{
	if (m_runnable)
	{
		m_runnable->Stop();
	}

	if (m_runnableController)
		m_runnableController->Kill(true);

	delete m_runnableController;
	m_runnableController = nullptr;

	delete m_runnable;
	m_runnable = nullptr;
}

void EvercoastAsyncStreamingDataDecoder::Receive(double timestamp, int64_t frameIndex, const uint8_t* data, size_t data_size, uint32_t metadata)
{
	if (m_baseDecoder->GetType() == DT_EvercoastVoxel)
	{
		VoxelDecodeThread* decodeThread = static_cast<VoxelDecodeThread*>(m_runnable);
		if (!decodeThread->AddEntry(timestamp, frameIndex, data, data_size))
		{
			UE_LOG(EvercoastDecoderLog, Warning, TEXT("Add entry to decoder failed. Buffer is full."));
		}
	}
	else
	{
		CortoDecodeThread* decodeThread = static_cast<CortoDecodeThread*>(m_runnable);
		if (!decodeThread->AddEntry(timestamp, frameIndex, data, data_size, metadata))
		{
			UE_LOG(EvercoastDecoderLog, Warning, TEXT("Add entry %lld -> %.3f to decoder failed. Buffer is full."), frameIndex, timestamp);
			decodeThread->_PrintBufferContents();
		}
	}
}

std::shared_ptr<GenericDecodeResult> EvercoastAsyncStreamingDataDecoder::PopResult()
{
	if (m_baseDecoder->GetType() == DT_EvercoastVoxel)
	{
		VoxelDecodeThread* decodeThread = static_cast<VoxelDecodeThread*>(m_runnable);
		return decodeThread->PopResult();
	}
	else
	{
		CortoDecodeThread* decodeThread = static_cast<CortoDecodeThread*>(m_runnable);
		return decodeThread->PopResult();
	}
}

std::shared_ptr<GenericDecodeResult> EvercoastAsyncStreamingDataDecoder::PeekResult(int offsetFromTop) const
{
	if (m_baseDecoder->GetType() == DT_EvercoastVoxel)
	{
		VoxelDecodeThread* decodeThread = static_cast<VoxelDecodeThread*>(m_runnable);
		return decodeThread->PeekResult(offsetFromTop);
	}
	else
	{
		CortoDecodeThread* decodeThread = static_cast<CortoDecodeThread*>(m_runnable);
		return decodeThread->PeekResult(offsetFromTop);
	}
}

void EvercoastAsyncStreamingDataDecoder::DisposeResult(std::shared_ptr<GenericDecodeResult> result)
{
	if (m_baseDecoder->GetType() == DT_EvercoastVoxel)
	{
		auto pResult = std::static_pointer_cast<EvercoastDecodeResult>(result);
		if (pResult->resultFrame != InvalidHandle)
		{
			release_voxel_frame_instance(pResult->resultFrame);
		}

		//result.reset();
	}
	else
	{
		// we don't want to destroy the result object since it contains vb/ib and is memory heavy.
	}
}

void EvercoastAsyncStreamingDataDecoder::FlushAndDisposeResults()
{
	if (m_baseDecoder->GetType() == DT_EvercoastVoxel)
	{
		VoxelDecodeThread* decodeThread = static_cast<VoxelDecodeThread*>(m_runnable);
		do
		{
			auto result = decodeThread->PopResult();
			while (result->DecodeSuccessful)
			{
				DisposeResult(result);
				result = decodeThread->PopResult();
			}
		} while (decodeThread->HasNewEntry());
	}
	else
	{
		CortoDecodeThread* decodeThread = static_cast<CortoDecodeThread*>(m_runnable);
		decodeThread->FlushAndDisposeResults();

		// as corto's result are all retained, it needs to be manually invalidated
		// ^^ oops looks like we shouldn't do this, at least do not reset the decode successful flag and index
		//decodeThread->InvalidateAllResults();
	}
	
	// Here we should have an underrun output buffer, which means all the results are flushed
}

void EvercoastAsyncStreamingDataDecoder::SetRequiresExternalData(bool required)
{
	if (m_baseDecoder->GetType() == DT_CortoMesh)
	{
		CortoDecodeThread* decodeThread = static_cast<CortoDecodeThread*>(m_runnable);
		decodeThread->SetRequiresExternalData(required);
	}
}


void EvercoastAsyncStreamingDataDecoder::ResizeBuffer(uint32_t bufferCount)
{
	if (m_baseDecoder->GetType() == DT_EvercoastVoxel)
	{
		VoxelDecodeThread* decodeThread = static_cast<VoxelDecodeThread*>(m_runnable);
		decodeThread->ResizeBuffer(bufferCount);
	}
	else
	{
		CortoDecodeThread* decodeThread = static_cast<CortoDecodeThread*>(m_runnable);
		decodeThread->ResizeBuffer(bufferCount);
	}
}