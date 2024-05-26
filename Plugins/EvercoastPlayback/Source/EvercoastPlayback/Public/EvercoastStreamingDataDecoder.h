#pragma once
#include <cstdint>
#include <memory>
#include "ec_decoder_compatibility.h"
#include "GenericDecoder.h"

class EVERCOASTPLAYBACK_API IEvercoastStreamingDataDecoder
{
public:
	virtual void ResizeBuffer(uint32_t bufferCount) = 0;
	virtual void Receive(double timestamp, int64_t frameIndex, const uint8_t* data, size_t data_size, uint32_t metadata) = 0;
	virtual std::shared_ptr<GenericDecodeResult> PopResult() = 0;
	virtual std::shared_ptr<GenericDecodeResult> PeekResult(int offsetFromTop = 0) const = 0;
	virtual void DisposeResult(std::shared_ptr<GenericDecodeResult> result) = 0;
	virtual void FlushAndDisposeResults() = 0;
	virtual void SetRequiresExternalData(bool required) = 0;
};
