#pragma once
#include <cstdint>
#include <memory>
#include "ec_decoder_compatibility.h"
#include "GenericDecoder.h"

class EVERCOASTPLAYBACK_API IEvercoastBaseDataDecoder
{
public:
	virtual void ResizeBuffer(uint32_t bufferCount, double halfFrameInterval) = 0;
	virtual void Receive(double timestamp, int64_t frameIndex, const uint8_t* data, size_t data_size, uint32_t metadata) = 0;
	virtual void FlushAndDisposeResults() = 0;
	virtual bool IsGoingToBeFull() const = 0;
};
