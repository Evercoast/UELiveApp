#pragma once

#include <memory>
#include "GenericDecoder.h"
#include "CortoDecoder.h"
#include "WebpDecoder.h"


// More flexible version, support deep copy
struct CortoImageUnifiedDecodeResult : public GenericDecodeResult
{
	std::shared_ptr<CortoDecodeResult>	meshResult;
	std::shared_ptr<WebpDecodeResult>	imgResult;

	CortoImageUnifiedDecodeResult(uint32_t initVertexReserved, uint32_t initTriangleReserved, int initWidth, int initHeight, uint8_t initBbp) :
		GenericDecodeResult(false, -1, -1)
	{
		meshResult = std::make_shared<CortoDecodeResult>(initVertexReserved, initTriangleReserved);
		imgResult = std::make_shared<WebpDecodeResult>(initWidth, initHeight, initBbp);
	}

	virtual ~CortoImageUnifiedDecodeResult() = default;

	CortoImageUnifiedDecodeResult(const CortoImageUnifiedDecodeResult& rhs) :
		GenericDecodeResult(rhs.DecodeSuccessful, rhs.frameTimestamp, rhs.frameIndex)
	{
		meshResult = std::make_shared<CortoDecodeResult>(*rhs.meshResult);
		imgResult = std::make_shared<WebpDecodeResult>(*rhs.imgResult);
	}

	void SyncWithMeshResult()
	{
		check(meshResult && meshResult->frameIndex >= 0);

		this->DecodeSuccessful = meshResult->DecodeSuccessful;
		this->frameIndex = meshResult->frameIndex;
		this->frameTimestamp = meshResult->frameTimestamp;

	}

	void InvalidateResult()
	{
		meshResult->InvalidateResult();
		imgResult->InvalidateResult();
	}

	virtual DecodeResultType GetType() const
	{
		return DecodeResultType::DRT_CortoMesh_WebpImage_Unified;
	}
};
