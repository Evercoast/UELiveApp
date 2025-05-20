#include "EvercoastAsyncStreamingDataDecoder.h"
#include <mutex>
#include "EvercoastVoxelDecoder.h"
#include "EvercoastEncodedDataFrame.h"
#include "GhostTreeFormatReader.h"
#include "CortoDecoder.h"
#include "WebpDecoder.h"
#include "CortoWebpUnifiedDecodeResult.h"
#include "Gaussian/EvercoastGaussianSplatDecoder.h"
#include "Gaussian/EvercoastGaussianSplatPassthroughResult.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "EvercoastPlaybackUtils.h"
#include <thread>
#include <chrono>
#include <queue>
#include <list>

EvercoastAsyncStreamingDataDecoder::ResultCache::ResultCache(int initialBufferCount) :
	m_resultStartIdx(0),
	m_resultEndIdx(0),
	m_bufferCount(initialBufferCount)
{
	m_resultArray = new std::shared_ptr<GenericDecodeResult>[initialBufferCount];
}

void EvercoastAsyncStreamingDataDecoder::ResultCache::Dispose()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (m_resultArray)
	{
		for (int i = 0; i < m_bufferCount; ++i)
		{
			auto pResult = m_resultArray[i];
			if (pResult)
				pResult->InvalidateResult();
			m_resultArray[i] = nullptr;
		}

		delete[] m_resultArray;
		m_resultArray = nullptr;
	}

	m_resultStartIdx = 0;
	m_resultEndIdx = 0;
}

void EvercoastAsyncStreamingDataDecoder::ResultCache::DisposeAndReinit()
{
	Dispose();
	m_resultArray = new std::shared_ptr<GenericDecodeResult>[m_bufferCount];
}

void EvercoastAsyncStreamingDataDecoder::ResultCache::Resize(uint32_t bufferCount)
{
	Dispose();

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	m_bufferCount = bufferCount;
	m_resultArray = new std::shared_ptr<GenericDecodeResult>[bufferCount];

	m_resultStartIdx = 0;
	m_resultEndIdx = 0;
}

void EvercoastAsyncStreamingDataDecoder::ResultCache::Add(std::shared_ptr<GenericDecodeResult> result)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (m_resultArray[m_resultEndIdx])
	{
		m_resultArray[m_resultEndIdx]->InvalidateResult(); // free memory if used before
	}

	m_resultArray[m_resultEndIdx] = result;
	m_resultEndIdx = (m_resultEndIdx + 1) % m_bufferCount;
}

std::shared_ptr<GenericDecodeResult> EvercoastAsyncStreamingDataDecoder::ResultCache::Prealloc()
{
	// no lock, requires a full Lock() beforehand

	auto result = m_resultArray[m_resultEndIdx];
	if (result)
	{
		result->InvalidateResult(); // free memory if used before
	}

	m_resultEndIdx = (m_resultEndIdx + 1) % m_bufferCount;

	return result;
}

void EvercoastAsyncStreamingDataDecoder::ResultCache::FillLastPrealloc(std::shared_ptr<GenericDecodeResult> newCacheObj)
{
	// no lock, requires a full Lock() beforehand

	int lastEndIdx = m_resultEndIdx - 1;
	if (lastEndIdx < 0)
	{
		lastEndIdx += m_bufferCount;
	}

	if (m_resultArray[lastEndIdx])
	{
		m_resultArray[lastEndIdx]->InvalidateResult();
	}

	m_resultArray[lastEndIdx] = newCacheObj;
}

void EvercoastAsyncStreamingDataDecoder::ResultCache::Lock()
{
	m_mutex.lock();
}

void EvercoastAsyncStreamingDataDecoder::ResultCache::Unlock()
{
	m_mutex.unlock();
}

std::shared_ptr<GenericDecodeResult> EvercoastAsyncStreamingDataDecoder::ResultCache::Query(double timestamp, double halfFrameInterval)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	for (int i = m_resultStartIdx; i != m_resultEndIdx; i = (i + 1) % m_bufferCount)
	{
		if (abs(m_resultArray[i]->frameTimestamp - timestamp) <= halfFrameInterval)
		{
			return m_resultArray[i];
		}
	}

	return nullptr;
}

// Maximum size will be m_bufferCount-1
int EvercoastAsyncStreamingDataDecoder::ResultCache::Size() const
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (m_resultStartIdx <= m_resultEndIdx)
	{
		return m_resultEndIdx - m_resultStartIdx;
	}
	else
	{
		return m_resultEndIdx + m_bufferCount - m_resultStartIdx;
	}

}

bool EvercoastAsyncStreamingDataDecoder::ResultCache::IsFull() const
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);
	if ((m_resultEndIdx + 1) % m_bufferCount == m_resultStartIdx)
		return true;

	return false;
}

bool EvercoastAsyncStreamingDataDecoder::ResultCache::IsEmpty() const
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);
	if (m_resultStartIdx == m_resultEndIdx)
		return true;

	return false;
}

bool EvercoastAsyncStreamingDataDecoder::ResultCache::IsGoingToBeFull(int futureResultCount) const
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	// can't fit
	if (futureResultCount >= m_bufferCount)
		return true;

	if (m_resultEndIdx >= m_resultStartIdx)
	{
		return (m_resultEndIdx + futureResultCount) >= m_resultStartIdx + m_bufferCount;
	}
	else// if (m_resultEndIdx < m_resultStartIdx)
	{
		return (m_resultEndIdx + futureResultCount) >= m_resultStartIdx;
	}
}


bool EvercoastAsyncStreamingDataDecoder::ResultCache::Trim(double medianTimestamp, double halfFrameInterval, int halfCacheWidth)
{
	bool trimmed = false;
	// halfFrameInterval == 0.5 / content_sample_rate
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	// find median
	int medianIdx = -1;
	for (int i = m_resultStartIdx; i != m_resultEndIdx; i = (i + 1) % m_bufferCount)
	{
		if (abs(m_resultArray[i]->frameTimestamp - medianTimestamp) <= halfFrameInterval)
		{
			medianIdx = i;
			break;
		}
	}

	if (medianIdx >= 0)
	{
		int medianDistanceToHead;
		// trim head
		if (medianIdx > m_resultStartIdx)
		{
			// trim
			if (medianIdx - halfCacheWidth > m_resultStartIdx)
			{
				m_resultStartIdx = medianIdx - halfCacheWidth;
				trimmed = true;
			}

			medianDistanceToHead = medianIdx - m_resultStartIdx;
		}
		else if (medianIdx < m_resultStartIdx)
		{
			// trim
			if (medianIdx + m_bufferCount - halfCacheWidth > m_resultStartIdx)
			{
				m_resultStartIdx = (medianIdx + m_bufferCount - halfCacheWidth) % m_bufferCount;
				trimmed = true;
			}

			medianDistanceToHead = medianIdx + m_bufferCount - m_resultStartIdx;
		}
		else
		{
			// median == start, no trimming
			medianDistanceToHead = 0;
		}

		// trim tail
		if (medianDistanceToHead >= halfCacheWidth) // only consider trim tail when head<->median has grown to size
		{
			if (medianIdx < m_resultEndIdx)
			{
				// trim
				if (medianIdx + halfCacheWidth < m_resultEndIdx)
				{
					m_resultEndIdx = medianIdx + halfCacheWidth;
					trimmed = true;
				}
			}
			else if (medianIdx > m_resultEndIdx)
			{
				// trim
				if (medianIdx + halfCacheWidth < m_resultEndIdx + m_bufferCount)
				{
					m_resultEndIdx = (medianIdx + halfCacheWidth - m_bufferCount) % m_bufferCount;
					trimmed = true;
				}
			}
			else
			{
				// median == end, no trimming
			}
		}

	}
	else
	{
		if (m_resultStartIdx != m_resultEndIdx) // non empty
		{
			// check if the median timestamp is totally running out of cache range
			int lastElementIdx = m_resultEndIdx - 1;
			if (lastElementIdx < 0)
			{
				lastElementIdx += m_bufferCount;
			}

			if (!m_resultArray[lastElementIdx] || medianTimestamp > m_resultArray[lastElementIdx]->frameTimestamp)
			{
				// reset
				DisposeAndReinit();

				UE_LOG(EvercoastVoxelDecoderLog, Verbose, TEXT("Geom Trimmed & Diposed"));
				return true;
			}
		}

	}

	if (trimmed)
	{
		UE_LOG(EvercoastVoxelDecoderLog, VeryVerbose, TEXT("Geom Trimmed: median: %.2f head(%d): %.2f -> tail(%d): %.2f"), medianTimestamp, 
			m_resultStartIdx, m_resultArray[m_resultStartIdx]->frameTimestamp, 
			m_resultEndIdx, m_resultEndIdx == 0 ? m_resultArray[m_bufferCount-1]->frameTimestamp : m_resultArray[m_resultEndIdx-1]->frameTimestamp);
	}

	return trimmed;

}

bool EvercoastAsyncStreamingDataDecoder::ResultCache::IsBeyond(double timestamp, double halfFrameInterval) const
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);
	if (m_resultStartIdx == m_resultEndIdx)
		return true; // empty

	int lastEndIdx = m_resultEndIdx - 1;
	if (lastEndIdx < 0)
	{
		lastEndIdx += m_bufferCount;
	}

	if (m_resultArray[lastEndIdx] && m_resultArray[lastEndIdx]->frameTimestamp + halfFrameInterval < timestamp)
	{
		return true;
	}

	return false;

}

EvercoastAsyncStreamingDataDecoder::ResultPresorter::ResultPresorter(ResultCache& resultCache, double frameInterval) :
	m_frameInterval(frameInterval), m_lastDeliveredTimestamp(-1.0), m_resultCache(resultCache)
{

}

/*
static void DebugPrintPresortResults(std::vector< std::shared_ptr<GenericDecodeResult>>& presortedResults)
{
	UE_LOG(EvercoastVoxelDecoderLog, Log, TEXT(">>>>"));
	for (int i = 0; i < presortedResults.size(); ++i)
	{
		UE_LOG(EvercoastVoxelDecoderLog, Log, TEXT("Presort[%d] = %.2f"), i, presortedResults[i]->frameTimestamp);
	}
	UE_LOG(EvercoastVoxelDecoderLog, Log, TEXT("<<<<"));
}
*/

void EvercoastAsyncStreamingDataDecoder::ResultPresorter::Add(std::shared_ptr<GenericDecodeResult> result)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	// first delivery
	if (m_lastDeliveredTimestamp < 0 && result->frameTimestamp == 0.0)
	{
		// Can force deliver the first frame
		ForceDeliver(result);
		m_lastDeliveredTimestamp = result->frameTimestamp;
	}
	else
	{
		// Need to insert to the presort cache before sending off
		auto it = std::find_if(m_presortedResults.begin(), m_presortedResults.end(), [result](std::shared_ptr<GenericDecodeResult> existingResult) {
				return existingResult->frameTimestamp > result->frameTimestamp;
			});

		if (it != m_presortedResults.end())
		{
			// Insert *after*
			m_presortedResults.insert(it, result);
		}
		else
		{
			m_presortedResults.push_back(result);
		}

		//DebugPrintPresortResults(m_presortedResults);

			
		CheckContinuityAndFeed();
	}
}

void EvercoastAsyncStreamingDataDecoder::ResultPresorter::ForceDeliver(std::shared_ptr<GenericDecodeResult> result)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	m_resultCache.Add(result);
}

void EvercoastAsyncStreamingDataDecoder::ResultPresorter::CheckContinuityAndFeed()
{
	if (m_lastDeliveredTimestamp < 0)
		return;

	// Check if the first element in the list is in-order
	// If so, deliver it
	while (m_presortedResults.size() > 0)
	{
		auto firstCandidateIt = m_presortedResults.begin();
		auto& firstCandidate = *firstCandidateIt;

		if (abs((firstCandidate->frameTimestamp - m_lastDeliveredTimestamp) - m_frameInterval) < 0.001)
		{
			ForceDeliver(firstCandidate);

			m_lastDeliveredTimestamp = firstCandidate->frameTimestamp;
			m_presortedResults.erase(firstCandidateIt);
		}
		else
		{
			// No longer having continuity, wait
			break;
		}
	}
}

void EvercoastAsyncStreamingDataDecoder::ResultPresorter::CleanupPresortedResults()
{
	for(auto it = m_presortedResults.begin(); it != m_presortedResults.end(); ++it)
	{
		auto pResult = *it;
		if (pResult)
		{
			pResult->InvalidateResult();
		}
	}

	m_presortedResults.clear();
	m_lastDeliveredTimestamp = -1.0;
}

void EvercoastAsyncStreamingDataDecoder::ResultPresorter::Dispose()
{
	m_resultCache.Dispose();

	CleanupPresortedResults();
}

void EvercoastAsyncStreamingDataDecoder::ResultPresorter::DisposeAndReinit()
{
	m_resultCache.DisposeAndReinit();

	CleanupPresortedResults();
}

class EvercoastSpzDecodeThread final : public FEvercoastGenericDecodeThread
{
public:
	EvercoastSpzDecodeThread(std::shared_ptr<IGenericDecoder> decoder, EvercoastAsyncStreamingDataDecoder::ResultPresorter& resultPresorter) :
		m_baseDecoder(decoder), m_running(false),
		m_resultPresorter(resultPresorter)
	{
	}

	~EvercoastSpzDecodeThread()
	{
	}

	bool Init() override
	{
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

			m_newEntrySemaphore.acquire();


			std::shared_ptr<EvercoastEncodedDataFrame> dataFrame;
			{
				std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);

				if (HasNewEntry())
				{
					
					dataFrame = m_localDataFrameList.front();
					m_localDataFrameList.pop();

					UE_LOG(EvercoastVoxelDecoderLog, Verbose, TEXT("SpzDecodeThread::Run HasNewEntry time: %.2f, Input Ring size: %d"), dataFrame->m_timestamp, m_localDataFrameList.size());
				}
			}

			if (dataFrame)
			{
#if 0
				EvercoastGaussianSplatDecodeOption decodeOption(true);
				if (m_baseDecoder->DecodeMemoryStream(dataFrame->m_data, dataFrame->m_dataSize, dataFrame->m_timestamp, dataFrame->m_frameIndex, &decodeOption))
				{
					auto result = m_baseDecoder->TakeResult();
					auto pResult = std::static_pointer_cast<EvercoastGaussianSplatDecodeResult>(result);

					m_resultPresorter.Add(pResult);
				}
#else
				EvercoastGaussianSplatDecodeOption decodeOption(false);
				if (m_baseDecoder->DecodeMemoryStream(dataFrame->m_data, dataFrame->m_dataSize, dataFrame->m_timestamp, dataFrame->m_frameIndex, &decodeOption))
				{
					auto result = m_baseDecoder->TakeResult();
					auto pResult = std::static_pointer_cast<EvercoastGaussianSplatPassthroughResult>(result);

					m_resultPresorter.Add(pResult);
				}
#endif
				else
				{
					UE_LOG(EvercoastVoxelDecoderLog, Warning, TEXT("Decode Gaussian failed"));
				}
			}


		}

		return 0;

	}

	void Stop() override
	{
		std::lock_guard<std::recursive_mutex> guard(m_controllerMutex);
		m_running = false;

		m_newEntrySemaphore.release();
	}

	void Exit() override
	{
		m_resultPresorter.Dispose();
	}

	bool HasNewEntry() const
	{
		// Removed the lock as it will be called along with explicit lock
		return !m_localDataFrameList.empty();
	}

	size_t GetNewEntryCount() const
	{
		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);
		return m_localDataFrameList.size();
	}

	void AddEntry(double timestamp, int64_t frameIndex, const uint8_t* data, size_t dataSize, uint32_t metadata)
	{
		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);

		UE_LOG(EvercoastVoxelDecoderLog, Verbose, TEXT("SpzDecodeThread::AddEntry %.2f"), timestamp);
		m_localDataFrameList.emplace(std::make_shared<EvercoastEncodedDataFrame>(timestamp, frameIndex, data, dataSize));

		m_newEntrySemaphore.release();

	}

	int GetPotentialResultCount() const
	{
		int potentialCount = 2;
		{
			std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);
			potentialCount += m_localDataFrameList.size();
		}

		return potentialCount;
	}

	void FlushAndDisposeResults()
	{
		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);

		// deplete input buffer
		while (!m_localDataFrameList.empty())
			m_localDataFrameList.pop();

		// drain output cache
		m_resultPresorter.DisposeAndReinit();

	}

private:
	std::shared_ptr<IGenericDecoder> m_baseDecoder;

	std::queue<std::shared_ptr<EvercoastEncodedDataFrame>> m_localDataFrameList;
	bool m_running;

	mutable std::recursive_mutex m_controllerMutex;
	mutable std::recursive_mutex m_inputMutex;

	CountingSemaphore m_newEntrySemaphore;
	EvercoastAsyncStreamingDataDecoder::ResultPresorter& m_resultPresorter;
};

// An asynchronised decode thread but no control over what timestamp the frame it can get
class VoxelDecodeThread final : public FEvercoastGenericDecodeThread
{
public:
	VoxelDecodeThread(std::shared_ptr<IGenericDecoder> decoder, EvercoastAsyncStreamingDataDecoder::ResultPresorter& resultPresorter) :
		m_baseDecoder(decoder), m_running(false),
		m_resultPresorter(resultPresorter)
	{
		auto voxelDecoder = std::static_pointer_cast<EvercoastVoxelDecoder>(m_baseDecoder);
		m_baseDefinition = voxelDecoder->GetDefaultDefinition();
		m_baseDefinition.fast_decode = true;
		m_baseDefinition.rgb_colourspace_conversion = false;
		m_baseDefinition.cartesian_space_conversion = true;
		m_baseDefinition.gfx_api_compatibility_mode = true;
		m_baseDefinition.half_float_coordinates = false;

	}

	~VoxelDecodeThread()
	{
	}

	bool Init() override
	{
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

			m_newEntrySemaphore.acquire();


			std::shared_ptr<EvercoastEncodedDataFrame> dataFrame;
			{
				std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);
			
				if (HasNewEntry())
				{
					UE_LOG(EvercoastVoxelDecoderLog, Verbose, TEXT("Decode: Input Ring size: %d"), m_localDataFrameList.size());
					dataFrame = m_localDataFrameList.front();
					m_localDataFrameList.pop();
				}
			}

			if (dataFrame)
			{
				EvercoastVoxelDecodeOption option(m_baseDefinition);
				if (m_baseDecoder->DecodeMemoryStream(dataFrame->m_data, dataFrame->m_dataSize, dataFrame->m_timestamp, dataFrame->m_frameIndex, &option))
				{
					auto result = m_baseDecoder->TakeResult();
					auto pResult = std::static_pointer_cast<EvercoastVoxelDecodeResult>(result);
					check(pResult->resultFrame);

					m_resultPresorter.Add(pResult);
				}
				else
				{
					UE_LOG(EvercoastVoxelDecoderLog, Warning, TEXT("Decode voxel failed"));
				}

			}


		}

		return 0;
	}

	void Stop() override
	{
		std::lock_guard<std::recursive_mutex> guard(m_controllerMutex);
		m_running = false;

		m_newEntrySemaphore.release();
	}

	void Exit() override
	{
		m_resultPresorter.Dispose();
	}

	bool HasNewEntry() const
	{
		// Removed the lock as it will be called along with explicit lock
		return !m_localDataFrameList.empty();
	}
	
	size_t GetNewEntryCount() const
	{
		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);
		return m_localDataFrameList.size();
	}

	// Always able to AddEntry() but will be limiting request next block based on whether output buffer is full
	// So the pending
	void AddEntry(double timestamp, int64_t frameIndex, const uint8_t* data, size_t dataSize, uint32_t metadata)
	{
		UE_LOG(EvercoastVoxelDecoderLog, Verbose, TEXT("AddEntry: %.2f Input Ring length: %d"), timestamp, m_localDataFrameList.size());

		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);
		m_localDataFrameList.emplace(std::make_shared<EvercoastEncodedDataFrame>(timestamp, frameIndex, data, dataSize));

		m_newEntrySemaphore.release();
	}


	int GetPotentialResultCount() const
	{
		// 1: there could be one result popped from input list, decoding in the middle, and not yet pushed to result cache
		// 2: there could be another one in the middle of the network streaming, but due to the one after one nature of GT's reading API, it cannot be more than one in transfer
		int potentialCount = 2; 
		{
			std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);
			potentialCount += m_localDataFrameList.size();
		}

		return potentialCount;
	}

	void FlushAndDisposeResults()
	{
		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);

		// deplete input buffer
		while(!m_localDataFrameList.empty())
			m_localDataFrameList.pop();

		// drain output cache
		m_resultPresorter.DisposeAndReinit();
	}

private:

	std::shared_ptr<IGenericDecoder> m_baseDecoder;
	Definition m_baseDefinition;

	std::queue<std::shared_ptr<EvercoastEncodedDataFrame>> m_localDataFrameList;
	bool m_running;

	mutable std::recursive_mutex m_controllerMutex;
	mutable std::recursive_mutex m_inputMutex;

	CountingSemaphore m_newEntrySemaphore;

	EvercoastAsyncStreamingDataDecoder::ResultPresorter& m_resultPresorter;
};


class CortoDecodeThread final : public FEvercoastGenericDecodeThread
{
	
public:
	CortoDecodeThread(std::shared_ptr<IGenericDecoder> mesh_decoder, std::shared_ptr<IGenericDecoder> image_decoder, EvercoastAsyncStreamingDataDecoder::ResultPresorter& resultPresorter) :
		m_cortoDecoder(std::static_pointer_cast<CortoDecoder>(mesh_decoder)), 
		m_webpDecoder(std::static_pointer_cast<WebpDecoder>(image_decoder)), 
		m_running(false), m_requiresExternalData(false),
		m_resultPresorter(resultPresorter)
	{
	}

	~CortoDecodeThread()
	{
		m_localDataFrameList.clear();
		m_cortoDecoder.reset();
		m_webpDecoder.reset();
	}

	bool Init() override
	{
		m_running = true;
		return m_cortoDecoder != nullptr;
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


			m_newEntrySemaphore.acquire();


			std::shared_ptr<ECMEncodedDataFrame> dataFrame;
			{
				std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);

				if (HasNewEntry())
				{


					int lastDecodedWidth = -1;
					int lastDecodedHeight = -1;

					UE_LOG(EvercoastVoxelDecoderLog, VeryVerbose, TEXT("Decode: Input Ring size: %d"), m_localDataFrameList.size());

					dataFrame = m_localDataFrameList.front();
					if (dataFrame->IsReady())
					{
						m_localDataFrameList.pop_front();
					}
					else
					{
						// do not proceed to decoding if data isn't fully received
						dataFrame = nullptr;
					}
				}
			}
			

			CortoDecodeOption option;
			if (dataFrame)
			{
				// FIXME: preallocate and reuse buffers!
				std::shared_ptr<CortoWebpUnifiedDecodeResult> unifiedResult = std::make_shared<CortoWebpUnifiedDecodeResult>(
					CortoDecoder::DEFAULT_VERTEX_COUNT, CortoDecoder::DEFAULT_TRIANGLE_COUNT, 1024, 1024, 32);

				m_cortoDecoder->SetReceivingResult(std::move(unifiedResult->meshResult));
				bool requireImageDecoding = !m_requiresExternalData;
				if (requireImageDecoding)
				{
					if (m_webpDecoder)
						m_webpDecoder->SetReceivingResult(std::move(unifiedResult->imgResult));

					if (m_cortoDecoder->DecodeMemoryStream(dataFrame->m_data, dataFrame->m_dataSize, dataFrame->m_timestamp, dataFrame->m_frameIndex, &option) &&
						m_webpDecoder->DecodeMemoryStream(dataFrame->m_imageData, dataFrame->m_imageDataSize, dataFrame->m_timestamp, dataFrame->m_frameIndex, &option))
					{
						unifiedResult->meshResult = std::static_pointer_cast<CortoDecodeResult>(m_cortoDecoder->TakeResult());
						unifiedResult->imgResult = std::static_pointer_cast<WebpDecodeResult>(m_webpDecoder->TakeResult());
						unifiedResult->SyncWithMeshResult();

						m_resultPresorter.Add(unifiedResult);
					}
					else
					{
						UE_LOG(EvercoastVoxelDecoderLog, Warning, TEXT("Decode mesh+image failed"));
						m_cortoDecoder->UnsetReceivingResult();
						if (m_webpDecoder)
							m_webpDecoder->UnsetReceivingResult();

					}
				}
				else
				{
					if (m_cortoDecoder->DecodeMemoryStream(dataFrame->m_data, dataFrame->m_dataSize, dataFrame->m_timestamp, dataFrame->m_frameIndex, &option))
					{
						unifiedResult->meshResult = std::static_pointer_cast<CortoDecodeResult>(m_cortoDecoder->TakeResult());
						unifiedResult->SyncWithMeshResult();
						
						m_resultPresorter.Add(unifiedResult);
					}
					else
					{
						UE_LOG(EvercoastVoxelDecoderLog, Warning, TEXT("Decode mesh failed"));

						m_cortoDecoder->UnsetReceivingResult();
					}
				}



				dataFrame->Invalidate();


				/*
				// The cache needs to be frozen, no more cursor moving or content changing
				m_resultCache.Lock();
				std::shared_ptr<CortoWebpUnifiedDecodeResult> unifiedResult = std::static_pointer_cast<CortoWebpUnifiedDecodeResult>(m_resultCache.Prealloc());
				if (!unifiedResult)
				{
					unifiedResult = std::make_shared<CortoWebpUnifiedDecodeResult>(
						CortoDecoder::DEFAULT_VERTEX_COUNT, CortoDecoder::DEFAULT_TRIANGLE_COUNT, 1024, 1024, 32);

					m_resultCache.FillLastPrealloc(unifiedResult);
				}

				m_cortoDecoder->SetReceivingResult(std::move(unifiedResult->meshResult));

				bool requireImageDecoding = !m_requiresExternalData;
				if (requireImageDecoding)
				{
					if (m_webpDecoder)
						m_webpDecoder->SetReceivingResult(std::move(unifiedResult->imgResult));

					if (m_cortoDecoder->DecodeMemoryStream(dataFrame->m_data, dataFrame->m_dataSize, dataFrame->m_timestamp, dataFrame->m_frameIndex, &option) &&
						m_webpDecoder->DecodeMemoryStream(dataFrame->m_imageData, dataFrame->m_imageDataSize, dataFrame->m_timestamp, dataFrame->m_frameIndex, &option))
					{
						unifiedResult->meshResult = std::static_pointer_cast<CortoDecodeResult>(m_cortoDecoder->TakeResult());
						unifiedResult->imgResult = std::static_pointer_cast<WebpDecodeResult>(m_webpDecoder->TakeResult());
						unifiedResult->SyncWithMeshResult();
							
						// No need to add to result cache, unifiedResult was preallocated
					}
					else
					{
						UE_LOG(EvercoastVoxelDecoderLog, Warning, TEXT("Decode mesh+image failed"));
						m_cortoDecoder->UnsetReceivingResult();
						if (m_webpDecoder)
							m_webpDecoder->UnsetReceivingResult();

					}
				}
				else
				{
					if (m_cortoDecoder->DecodeMemoryStream(dataFrame->m_data, dataFrame->m_dataSize, dataFrame->m_timestamp, dataFrame->m_frameIndex, &option))
					{
						unifiedResult->meshResult = std::static_pointer_cast<CortoDecodeResult>(m_cortoDecoder->TakeResult());
						unifiedResult->SyncWithMeshResult();
						// No need to add to result cache, unifiedResult was preallocated
					}
					else
					{
						UE_LOG(EvercoastVoxelDecoderLog, Warning, TEXT("Decode mesh failed"));

						m_cortoDecoder->UnsetReceivingResult();
					}
				}

				m_resultCache.Unlock();

				dataFrame->Invalidate();
				*/
			}
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

			m_newEntrySemaphore.release();
		}
	}

	void Exit() override
	{
#if ENGINE_MAJOR_VERSION == 5
		FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
#endif
		m_localDataFrameList.clear();
		m_resultPresorter.Dispose();

		m_cortoDecoder.reset();
		m_webpDecoder.reset();

	}

	/*
	void _PrintBufferContents()
	{
		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);
		for (int i = 0; i < m_bufferCount; ++i)
		{
			UE_LOG(EvercoastVoxelDecoderLog, Verbose, TEXT("Full input_buffer[%d] = %lld mesh ready: %d img ready: %d"), i, m_localDataFrameList[i]->m_frameIndex, m_localDataFrameList[i]->m_data != nullptr, m_localDataFrameList[i]->m_imageData != nullptr);
		}
	}
	*/

	bool HasNewEntry() const
	{
		// Removed lock, will be called along with explicit lock
		return !m_localDataFrameList.empty();
	}

	size_t GetNewEntryCount() const
	{
		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);
		return m_localDataFrameList.size();
	}

	std::shared_ptr<ECMEncodedDataFrame> FindLocalDataByFrameIndex(int64_t frameIndex)
	{
		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);

		auto it = m_localDataFrameList.begin();
		while(it != m_localDataFrameList.end())
		{
			if (*it && (*it)->m_frameIndex == frameIndex)
				return *it;

			++it;
		}

		return nullptr;
	}

	virtual void AddEntry(double timestamp, int64_t frameIndex, const uint8_t* data, size_t dataSize, uint32_t metadata) override
	{
#if ENGINE_MAJOR_VERSION == 5
		FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
#endif
		bool isImage = (metadata == UGhostTreeFormatReader::DECODE_META_IMAGE_WEBP);

		// Check if we need to update existing input frames first
		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);
		std::shared_ptr<ECMEncodedDataFrame> existingFrame = FindLocalDataByFrameIndex(frameIndex);
		if (existingFrame)
		{
			existingFrame->UpdateData(data, dataSize, isImage);
			UE_LOG(EvercoastVoxelDecoderLog, Verbose, TEXT("AddEntry(update) input_buffer = %lld %s"), frameIndex, isImage ? TEXT("image_data") : TEXT("mesh_data"));

			m_newEntrySemaphore.release();
			return;
		}


		m_localDataFrameList.emplace_back(std::make_shared<ECMEncodedDataFrame>(timestamp, frameIndex, data, dataSize, !m_requiresExternalData, isImage));

		UE_LOG(EvercoastVoxelDecoderLog, Verbose, TEXT("AddEntry(new) input_buffer = %.2f %s"), timestamp, isImage ? TEXT("image_data") : TEXT("mesh_data"));

		m_newEntrySemaphore.release();
	}

	int GetPotentialResultCount() const
	{
		int potentialCount = 2;
		{
			std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);
			potentialCount += m_localDataFrameList.size();
		}
		return potentialCount;
	}

	void FlushAndDisposeResults()
	{
		std::lock_guard<std::recursive_mutex> guardInput(m_inputMutex);

		m_localDataFrameList.clear();

		m_resultPresorter.DisposeAndReinit();
	}

private:


	std::shared_ptr<CortoDecoder> m_cortoDecoder;
	std::shared_ptr<WebpDecoder> m_webpDecoder;

	std::list<std::shared_ptr<ECMEncodedDataFrame>> m_localDataFrameList;
	bool m_running;
	bool m_requiresExternalData;

	mutable std::recursive_mutex m_controllerMutex;
	mutable std::recursive_mutex m_inputMutex;
	CountingSemaphore m_newEntrySemaphore;

	EvercoastAsyncStreamingDataDecoder::ResultPresorter& m_resultPresorter;
};


EvercoastAsyncStreamingDataDecoder::EvercoastAsyncStreamingDataDecoder(DecoderType decoderType) :
	m_resultCache(DEFAULT_BUFFER_COUNT), m_decoderType(decoderType)
{
	// Init has been delayed to when we can know frame interval
}

EvercoastAsyncStreamingDataDecoder::~EvercoastAsyncStreamingDataDecoder()
{
	Deinit();
}


void EvercoastAsyncStreamingDataDecoder::Init(double frameInterval, int maxThreadCount)
{
	m_resultPresorter = new ResultPresorter(m_resultCache, frameInterval);

	if (m_decoderType == DT_EvercoastVoxel)
	{
		for (int i = 0; i < maxThreadCount; ++i)
		{
			auto voxelDecoder = EvercoastVoxelDecoder::Create();
			auto decodeWorker = new VoxelDecodeThread(voxelDecoder, *m_resultPresorter);
			m_decodeWorkers.push_back(decodeWorker);

			FString name = FString::Format(TEXT("Voxel Decode Thread {0}"), { i + 1 });
			m_decodeWorkerControllers.push_back(FRunnableThread::Create(decodeWorker, *name));
		}
	}
	else if (m_decoderType == DT_EvercoastSpz)
	{
		for (int i = 0; i < maxThreadCount; ++i)
		{
			auto gaussianDecoder = EvercoastGaussianSplatDecoder::Create();
			auto decodeWorker = new EvercoastSpzDecodeThread(gaussianDecoder, *m_resultPresorter);
			m_decodeWorkers.push_back(decodeWorker);

			FString name = FString::Format(TEXT("Gaussian Splat Decode Thread {0}"), { i + 1 });
			m_decodeWorkerControllers.push_back(FRunnableThread::Create(decodeWorker, *name));
		}
	}
	else if (m_decoderType == DT_CortoMesh)
	{
		for (int i = 0; i < maxThreadCount; ++i)
		{
			auto cortoDecoder = CortoDecoder::Create();
			auto webpDecoder = WebpDecoder::Create();

			auto decodeWorker = new CortoDecodeThread(cortoDecoder, webpDecoder, *m_resultPresorter);
			m_decodeWorkers.push_back(decodeWorker);

			FString name = FString::Format(TEXT("Corto Decode Thread {0}"), { i + 1 });
			m_decodeWorkerControllers.push_back(FRunnableThread::Create(decodeWorker, *name));
		}
	}
}

void EvercoastAsyncStreamingDataDecoder::Deinit()
{
	for (auto it = m_decodeWorkers.begin(); it != m_decodeWorkers.end(); ++it)
	{
		auto thread = (*it);
		thread->Stop();
	}

	for (auto it = m_decodeWorkerControllers.begin(); it != m_decodeWorkerControllers.end(); ++it)
	{
		auto threadController = *it;
		threadController->Kill(true);
		delete threadController;
	}

	for (auto it = m_decodeWorkers.begin(); it != m_decodeWorkers.end(); ++it)
	{
		auto thread = (*it);
		delete thread;
	}



	m_decodeWorkers.clear();
	m_decodeWorkerControllers.clear();

	delete m_resultPresorter;
	m_resultPresorter = nullptr;
}

FEvercoastGenericDecodeThread* EvercoastAsyncStreamingDataDecoder::FindLeastJobWorker()
{
	int foundIndex = -1;
	size_t leastJobCount = (size_t)-1;
	for (int i = 0; i < m_decodeWorkers.size(); ++i)
	{
		size_t jobCount = m_decodeWorkers[i]->GetNewEntryCount();
		if (jobCount < leastJobCount)
		{
			leastJobCount = jobCount;
			foundIndex = i;
		}
	}

	// Pretty much should never happen, unless the job count overflows
	if (foundIndex < 0)
		return nullptr;

	return m_decodeWorkers[foundIndex];
}

void EvercoastAsyncStreamingDataDecoder::Receive(double timestamp, int64_t frameIndex, const uint8_t* data, size_t data_size, uint32_t metadata)
{
	UE_LOG(EvercoastVoxelDecoderLog, Verbose, TEXT("AsyncStreamingDataDecoder::Receive %.2f"), timestamp);

	FEvercoastGenericDecodeThread* decodeWorker = FindLeastJobWorker();
	decodeWorker->AddEntry(timestamp, frameIndex, data, data_size, metadata);
}

std::shared_ptr<GenericDecodeResult> EvercoastAsyncStreamingDataDecoder::QueryResult(double timestamp)
{
	return m_resultCache.Query(timestamp, m_halfFrameInterval);
}

std::shared_ptr<GenericDecodeResult> EvercoastAsyncStreamingDataDecoder::QueryResultAfterTimestamp(double afterTimestamp)
{
	if (afterTimestamp < 0)
	{
		return nullptr;
	}
	// Query for one frame after "afterTimestamp"
	return m_resultCache.Query(afterTimestamp + m_halfFrameInterval*2.0, m_halfFrameInterval);
}

bool EvercoastAsyncStreamingDataDecoder::IsTimestampBeyondCache(double timestamp)
{
	return m_resultCache.IsBeyond(timestamp, m_halfFrameInterval);
}

bool EvercoastAsyncStreamingDataDecoder::TrimCache(double medianTimestamp)
{
	return m_resultCache.Trim(medianTimestamp, m_halfFrameInterval, m_halfCacheWidth);
}

void EvercoastAsyncStreamingDataDecoder::FlushAndDisposeResults()
{
	for (auto it = m_decodeWorkers.begin(); it != m_decodeWorkers.end(); ++it)
	{
		(*it)->FlushAndDisposeResults();
	}
}

void EvercoastAsyncStreamingDataDecoder::SetRequiresExternalData(bool required)
{
	if (m_decoderType == DT_CortoMesh)
	{
		for (auto it = m_decodeWorkers.begin(); it != m_decodeWorkers.end(); ++it)
		{
			CortoDecodeThread* decodeWorker = static_cast<CortoDecodeThread*>(*it);
			decodeWorker->SetRequiresExternalData(required);
		}
	}
}


void EvercoastAsyncStreamingDataDecoder::ResizeBuffer(uint32_t bufferCount, double halfFrameInterval)
{
	m_resultCache.Resize(bufferCount);

	m_halfCacheWidth = bufferCount / 2;
	m_halfFrameInterval = halfFrameInterval;

	// FIXME: supply maximum threads but Presorter seems to have problem when sending first a few frames when loops back to the beginning
	Init(halfFrameInterval * 2.0, 1);// FGenericPlatformMisc::NumberOfCoresIncludingHyperthreads());
}


bool EvercoastAsyncStreamingDataDecoder::IsGoingToBeFull() const
{
	int potentialResultCountFromWorkers = 0;
	for (auto it = m_decodeWorkers.begin(); it != m_decodeWorkers.end(); ++it)
	{
		potentialResultCountFromWorkers += (*it)->GetPotentialResultCount();
	}

	return m_resultCache.IsGoingToBeFull(potentialResultCountFromWorkers);
}