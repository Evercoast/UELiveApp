#pragma once

#include <memory>
#include "GenericDecoder.h"
#include "CortoDecoder.h"
#include "WebpDecoder.h"
#include "Engine/Texture.h"

struct CortoWebpUnifiedDecodeResult : public GenericDecodeResult
{
	std::shared_ptr<CortoDecodeResult>	meshResult;
	std::shared_ptr<WebpDecodeResult>	imgResult;
	UTexture* videoTextureResult;

	CortoWebpUnifiedDecodeResult(uint32_t initVertexReserved, uint32_t initTriangleReserved, int initWidth, int initHeight, uint8_t initBbp) :
		GenericDecodeResult(false, -1, -1), videoTextureResult(nullptr)
	{
		meshResult = std::make_shared<CortoDecodeResult>(initVertexReserved, initTriangleReserved);
		imgResult = std::make_shared<WebpDecodeResult>(initWidth, initHeight, initBbp);
	}

	virtual ~CortoWebpUnifiedDecodeResult() = default;

	void Lock() const
	{
		meshResult->Lock();
		imgResult->Lock();
	}

	void Unlock() const
	{
		imgResult->Unlock();
		meshResult->Unlock();
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
		videoTextureResult = nullptr;
	}

	virtual DecodeResultType GetType() const
	{
		return DecodeResultType::DRT_CortoMesh_WebpImage_Unified;
	}
};
