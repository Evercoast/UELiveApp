#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include "EvercoastStreamingDataDecoder.h"
#include "ec_decoder_compatibility.h"
#include "GenericDecoder.h"

class EvercoastDecoder;
struct EvercoastLocalDataFrame;
struct ECMLocalDataFrame;
class EvercoastDecodeResult;
class CortoDecodeResult;
class WebpDecodeResult;
struct CortoWebpUnifiedDecodeResult;
class EVERCOASTPLAYBACK_API EvercoastBasicStreamingDataDecoder : public IEvercoastStreamingDataDecoder
{
public:
	static constexpr int DEFAULT_BUFFER_COUNT = 30;
	EvercoastBasicStreamingDataDecoder(std::shared_ptr<IGenericDecoder> base_decoder, std::shared_ptr<IGenericDecoder> aux_decoder);
	virtual ~EvercoastBasicStreamingDataDecoder();
	// ~Start of IEvercoastStreamingDataDecoder~
	virtual void Receive(double timestamp, int64_t frameIndex, const uint8_t* data, size_t data_size, uint32_t metadata);
	virtual std::shared_ptr<GenericDecodeResult> PopResult() override;
	virtual std::shared_ptr<GenericDecodeResult> PeekResult(int offsetFromTop) const override;
	virtual void DisposeResult(std::shared_ptr<GenericDecodeResult> result) override;
	virtual void FlushAndDisposeResults() override;
	virtual void SetRequiresExternalData(bool required) override;
	virtual void ResizeBuffer(uint32_t bufferCount) override;
	// ~End of IEvercoastStreamingDataDecoder~

	

private:
	void Deinit();

	std::shared_ptr<GenericDecodeResult> CreateEmptyResult() const;
	bool StoreVoxelResult(std::shared_ptr<GenericDecodeResult> result);
	bool IsResultQueueFull() const;
	bool IsResultQueueEmpty() const;
	int GetResultQueueSize() const;

	std::shared_ptr<IGenericDecoder> m_baseDecoder;
	std::shared_ptr<IGenericDecoder> m_auxDecoder;
	// Evercoast voxel decoder
	Definition m_baseDefinition;

	// ECV
	std::shared_ptr<EvercoastLocalDataFrame> m_localVoxelDataFrame;
	// ECM
	std::shared_ptr<ECMLocalDataFrame> m_localMeshDataFrame;
	bool m_requiresExternalData;

	std::vector<std::shared_ptr<CortoWebpUnifiedDecodeResult>> m_cortoResultQueue;
	std::vector<std::shared_ptr<EvercoastDecodeResult>> m_voxelResultQueue;
	
	int m_queueHead;
	int m_queueTail;
	uint32_t m_bufferCount;
};
