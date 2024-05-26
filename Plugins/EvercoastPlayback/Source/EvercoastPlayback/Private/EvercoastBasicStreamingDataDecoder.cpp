#include "EvercoastBasicStreamingDataDecoder.h"
#include "EvercoastDecoder.h"
#include "CortoDecoder.h"
#include "WebpDecoder.h"
#include "CortoWebpUnifiedDecodeResult.h"
#include "EvercoastLocalDataFrame.h"
#include "EvercoastStreamingDataUploader.h"
#include "GhostTreeFormatReader.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

EvercoastBasicStreamingDataDecoder::EvercoastBasicStreamingDataDecoder(std::shared_ptr<IGenericDecoder> base_decoder, std::shared_ptr<IGenericDecoder> aux_decoder) :
	m_baseDecoder(base_decoder),
	m_auxDecoder(aux_decoder),
	m_requiresExternalData(false),
	m_queueHead(0),
	m_queueTail(0),
	m_bufferCount(DEFAULT_BUFFER_COUNT)
{
	m_voxelResultQueue.resize(m_bufferCount);
	m_cortoResultQueue.resize(m_bufferCount);

	if (m_baseDecoder->GetType() == DT_EvercoastVoxel)
	{
		auto voxelDecoder = std::static_pointer_cast<EvercoastDecoder>(base_decoder);
		m_baseDefinition = voxelDecoder->GetDefaultDefinition();
		m_baseDefinition.fast_decode = true;
		m_baseDefinition.rgb_colourspace_conversion = false;
		m_baseDefinition.cartesian_space_conversion = true;
		m_baseDefinition.gfx_api_compatibility_mode = true;
		m_baseDefinition.half_float_coordinates = false;
	}
	else
	{
		// cache all the mesh decoder results
		for (auto i = 0; i < m_cortoResultQueue.size(); ++i)
		{
			m_cortoResultQueue[i] = std::make_shared<CortoWebpUnifiedDecodeResult>(CortoDecoder::DEFAULT_VERTEX_COUNT, CortoDecoder::DEFAULT_TRIANGLE_COUNT, 1024, 1024, 32);
		}
	}

	
}

EvercoastBasicStreamingDataDecoder::~EvercoastBasicStreamingDataDecoder()
{
	Deinit();
}

void EvercoastBasicStreamingDataDecoder::Deinit()
{
	m_voxelResultQueue.clear();
	m_cortoResultQueue.clear();
}

std::shared_ptr<GenericDecodeResult> EvercoastBasicStreamingDataDecoder::CreateEmptyResult() const
{
	if (m_baseDecoder->GetType() == DT_EvercoastVoxel)
	{
		return std::make_shared<EvercoastDecodeResult>(false, -1.0, -1, InvalidHandle);
	}
	else
	{
		return std::make_shared<CortoWebpUnifiedDecodeResult>(0, 0, 0, 0, 0);
	}
}

bool EvercoastBasicStreamingDataDecoder::StoreVoxelResult(std::shared_ptr<GenericDecodeResult> result)
{
	if (!IsResultQueueFull())
	{
		m_voxelResultQueue[m_queueTail] = std::static_pointer_cast<EvercoastDecodeResult>(result);

		m_queueTail = (m_queueTail + 1) % m_bufferCount;
		return true;
	}

	return false;
}

int EvercoastBasicStreamingDataDecoder::GetResultQueueSize() const
{
	if (m_queueTail >= m_queueHead)
		return m_queueTail - m_queueHead;

	else
		return m_queueTail + m_bufferCount - m_queueHead;
}

bool EvercoastBasicStreamingDataDecoder::IsResultQueueFull() const
{
	return m_queueTail + 1 == m_queueHead;
}

bool EvercoastBasicStreamingDataDecoder::IsResultQueueEmpty() const
{
	return m_queueTail == m_queueHead;
}

void EvercoastBasicStreamingDataDecoder::Receive(double timestamp, int64_t frameIndex, const uint8_t* data, size_t data_size, uint32_t metadata)
{
	// Sync-ed decoding
	if (m_baseDecoder->GetType() == DT_EvercoastVoxel)
	{
		// copy the to-decode-data to local
		m_localVoxelDataFrame = std::make_shared<EvercoastLocalDataFrame>(timestamp, frameIndex, data, data_size);

		const uint8_t* dataToDecode = m_localVoxelDataFrame->m_data;;
		size_t dataSizeToDecode = m_localVoxelDataFrame->m_dataSize;;

		EvercoastDecodeOption option(m_baseDefinition);
		if (!m_baseDecoder->DecodeMemoryStream(dataToDecode, dataSizeToDecode, timestamp, frameIndex, &option))
		{
			UE_LOG(EvercoastDecoderLog, Warning, TEXT("Decode from memory failed"));
			return;
		}
		else
		{
			if (!StoreVoxelResult(m_baseDecoder->GetResult()))
			{
				UE_LOG(EvercoastDecoderLog, Warning, TEXT("Store result failed. Decode result queue is full."));
			}
		}
	}
	else
	{
		bool isImage = (metadata == UGhostTreeFormatReader::DECODE_META_IMAGE_WEBP);
		bool requiresImageDecoding = !m_requiresExternalData;
		if (m_localMeshDataFrame && m_localMeshDataFrame->m_frameIndex == frameIndex)
		{
			m_localMeshDataFrame->UpdateData(data, data_size, isImage);
		}
		else
		{
			m_localMeshDataFrame = std::make_shared<ECMLocalDataFrame>(timestamp, frameIndex, data, data_size, requiresImageDecoding, isImage);
		}

		if (m_localMeshDataFrame->IsReady())
		{

			const uint8_t* dataToDecode = m_localMeshDataFrame->m_data;
			size_t dataSizeToDecode = m_localMeshDataFrame->m_dataSize;

			CortoDecodeOption option;
			if (IsResultQueueFull())
			{
				UE_LOG(EvercoastDecoderLog, Warning, TEXT("Cannot initiate decoding. Decode result queue already full."));
			}
			else
			{
				int currentResultIndex = m_queueTail;

				auto cortoDecoder = std::static_pointer_cast<CortoDecoder>(m_baseDecoder);
				auto webpDecoder = std::static_pointer_cast<WebpDecoder>(m_auxDecoder);

				{
					cortoDecoder->SetReceivingResult(
						std::static_pointer_cast<CortoDecodeResult>(m_cortoResultQueue[currentResultIndex]->meshResult));

					if (!cortoDecoder->DecodeMemoryStream(dataToDecode, dataSizeToDecode, timestamp, frameIndex, &option))
					{
						UE_LOG(EvercoastDecoderLog, Warning, TEXT("Decode mesh from memory failed"));
					}
				}


				if (requiresImageDecoding)
				{
					dataToDecode = m_localMeshDataFrame->m_imageData;
					dataSizeToDecode = m_localMeshDataFrame->m_imageDataSize;

					webpDecoder->SetReceivingResult(
						std::static_pointer_cast<WebpDecodeResult>(m_cortoResultQueue[currentResultIndex]->imgResult));

					if (!webpDecoder->DecodeMemoryStream(dataToDecode, dataSizeToDecode, timestamp, frameIndex, &option))
					{
						UE_LOG(EvercoastDecoderLog, Warning, TEXT("Decode image from memory failed"));
					}
				}

				m_cortoResultQueue[currentResultIndex]->SyncWithMeshResult();

				// increment
				m_queueTail = (m_queueTail + 1) % m_bufferCount;
			}
		}
			
	}
}


std::shared_ptr<GenericDecodeResult> EvercoastBasicStreamingDataDecoder::PopResult()
{
	if (IsResultQueueEmpty())
		return CreateEmptyResult();

	if (m_baseDecoder->GetType() == DT_EvercoastVoxel)
	{
		auto result = m_voxelResultQueue[m_queueHead];
		m_queueHead = (m_queueHead + 1) % m_bufferCount;
		return result;
	}
	else
	{
		auto result = m_cortoResultQueue[m_queueHead];
		m_queueHead = (m_queueHead + 1) % m_bufferCount;
		return result;
	}
}

std::shared_ptr<GenericDecodeResult> EvercoastBasicStreamingDataDecoder::PeekResult(int offsetFromTop) const
{
	if (offsetFromTop == 0)
	{
		if (IsResultQueueEmpty())
			return CreateEmptyResult();

		if (m_baseDecoder->GetType() == DT_EvercoastVoxel)
		{
			return m_voxelResultQueue[m_queueHead];
		}
		else
		{
			return m_cortoResultQueue[m_queueHead];
		}
	}
	else
	{
		if (GetResultQueueSize() <= offsetFromTop)
			return CreateEmptyResult();
		
		if (m_baseDecoder->GetType() == DT_EvercoastVoxel)
		{
			return m_voxelResultQueue[(m_queueHead + offsetFromTop) % m_bufferCount];
		}
		else
		{
			return m_cortoResultQueue[(m_queueHead + offsetFromTop) % m_bufferCount];
		}
	}
}


void EvercoastBasicStreamingDataDecoder::DisposeResult(std::shared_ptr<GenericDecodeResult> result)
{
	if (m_baseDecoder->GetType() == DT_EvercoastVoxel)
	{
		if (result->GetType() == DRT_EvercoastVoxel)
		{
			auto pResult = std::static_pointer_cast<EvercoastDecodeResult>(result);
			if (pResult && pResult->resultFrame != InvalidHandle)
			{
				release_voxel_frame_instance(pResult->resultFrame);
				pResult->resultFrame = InvalidHandle; // invalidate it otherwise will be released incorrectly next time
			}
		}
	}
	else
	{
		// we don't want to destroy the result object since it contains vb/ib and is memory heavy.
	}
}

void EvercoastBasicStreamingDataDecoder::FlushAndDisposeResults()
{
	// loop from head to tail and dispose
	while (m_queueTail != m_queueHead)
	{
		int index = m_queueHead;

		if (m_baseDecoder->GetType() == DT_EvercoastVoxel)
		{
			if (m_voxelResultQueue[index]->DecodeSuccessful)
			{
				DisposeResult(m_voxelResultQueue[index]);
				m_voxelResultQueue[index]->InvalidateResult();
			}
		}
		else
		{
			if (m_cortoResultQueue[index]->DecodeSuccessful)
			{
				DisposeResult(m_cortoResultQueue[index]);
				m_cortoResultQueue[index]->InvalidateResult();
			}
		}
		m_queueHead = (m_queueHead + 1) % m_bufferCount;
	}

	m_queueTail = 0;
	m_queueHead = 0;
}


void EvercoastBasicStreamingDataDecoder::SetRequiresExternalData(bool required)
{
	m_requiresExternalData = required;
}


void EvercoastBasicStreamingDataDecoder::ResizeBuffer(uint32_t bufferCount)
{
	m_bufferCount = bufferCount;
	m_voxelResultQueue.resize(m_bufferCount);
	m_cortoResultQueue.resize(m_bufferCount);

	if (m_baseDecoder->GetType() != DT_EvercoastVoxel)
	{
		// cache all the mesh decoder results
		for (auto i = 0; i < m_cortoResultQueue.size(); ++i)
		{
			m_cortoResultQueue[i] = std::make_shared<CortoWebpUnifiedDecodeResult>(CortoDecoder::DEFAULT_VERTEX_COUNT, CortoDecoder::DEFAULT_TRIANGLE_COUNT, 1024, 1024, 32);
		}
	}
}