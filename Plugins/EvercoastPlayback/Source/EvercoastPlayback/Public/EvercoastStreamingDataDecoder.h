#pragma once
#include <cstdint>
#include <memory>
#include "ec_decoder_compatibility.h"
#include "GenericDecoder.h"
#include "EvercoastBaseDataDecoder.h"

class EVERCOASTPLAYBACK_API IEvercoastStreamingDataDecoder : public IEvercoastBaseDataDecoder
{
public:
	virtual std::shared_ptr<GenericDecodeResult> QueryResult(double timestamp) = 0;
	virtual std::shared_ptr<GenericDecodeResult> QueryResultAfterTimestamp(double afterTimestamp) = 0;
	virtual bool IsTimestampBeyondCache(double timestamp) = 0;
	virtual bool TrimCache(double medianTimestamp) = 0;
	virtual void SetRequiresExternalData(bool required) = 0;
};
