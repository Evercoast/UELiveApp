#pragma once

#include <memory>
#include <map>
#include "ec_decoder_compatibility.h"
#include "CoreMinimal.h"
#include "GenericDecoder.h"


DECLARE_LOG_CATEGORY_EXTERN(EvercoastVoxelDecoderLog, Log, All);

constexpr uint32_t DECODER_MAX_VOXEL_COUNT = 2048 * 2048;
constexpr uint32_t DECODER_POSITION_ELEMENT_SIZE = 8;
constexpr uint32_t DECODER_COLOUR_ELEMENT_SIZE = 4;

class EVERCOASTPLAYBACK_API EvercoastVoxelDecodeOption : public GenericDecodeOption
{
public:
	Definition definition;
	EvercoastVoxelDecodeOption(Definition def) : definition(def)
	{}
};

class EVERCOASTPLAYBACK_API EvercoastVoxelDecodeResult : public GenericDecodeResult
{
public:
    GTHandle resultFrame;
	EvercoastVoxelDecodeResult(bool success, double timestamp, int64_t index, GTHandle handle) :
		GenericDecodeResult(success, timestamp, index),
		resultFrame(handle)
	{}

	virtual DecodeResultType GetType() const override
	{
		return DecodeResultType::DRT_EvercoastVoxel;
	}

	virtual void InvalidateResult() override
	{
		GenericDecodeResult::InvalidateResult();

		if (resultFrame != InvalidHandle)
		{
			release_voxel_frame_instance(resultFrame);
			resultFrame = InvalidHandle;
		}
	}
};

class EVERCOASTPLAYBACK_API EvercoastVoxelDecoder : public IGenericDecoder
{
public:
	static std::shared_ptr<EvercoastVoxelDecoder> Create();
	virtual ~EvercoastVoxelDecoder();

	Definition GetDefaultDefinition();
	virtual DecoderType GetType() const override;
	virtual bool DecodeMemoryStream(const uint8_t* stream, size_t stream_size, double timestamp, int64_t frameIndex, GenericDecodeOption* option) override;
	virtual std::shared_ptr<GenericDecodeResult> TakeResult() override;
private:
	EvercoastVoxelDecoder(GTHandle decoder_interface);

	static bool s_initialised;

    GTHandle m_interface;
	std::shared_ptr<EvercoastVoxelDecodeResult> m_result;
	//std::map<GTHandle, int32_t> m_frameRegistry;
};

