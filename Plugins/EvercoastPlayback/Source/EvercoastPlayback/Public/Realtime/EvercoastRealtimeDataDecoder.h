#pragma once
#include <cstdint>
#include <memory>
#include "ec_decoder_compatibility.h"
#include "GenericDecoder.h"
#include "EvercoastBaseDataDecoder.h"

class EVERCOASTPLAYBACK_API IEvercoastRealtimeDataDecoder : public IEvercoastBaseDataDecoder
{
public:
	virtual std::shared_ptr<GenericDecodeResult> PopResult() = 0;
	virtual std::shared_ptr<GenericDecodeResult> PeekResult(int offsetFromTop = 0) const = 0;
	virtual void DisposeResult(std::shared_ptr<GenericDecodeResult> result) = 0;
};
