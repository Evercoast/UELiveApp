#pragma once
#include <memory>
#include "ec_decoder_compatibility.h"
#include "GenericDecoder.h"

class FEvercoastVoxelSceneProxy;
class EVERCOASTPLAYBACK_API IEvercoastStreamingDataUploader
{
public:
	virtual void Upload(const GenericDecodeResult* pVoxelDecodeResult) = 0;
	virtual bool ForceUpload() = 0;
	virtual bool IsDataDirty() const = 0;
	virtual void MarkDataDirty() = 0;
	virtual void ReleaseLocalResource() = 0;
};
