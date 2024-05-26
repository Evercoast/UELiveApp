#pragma once

#include <memory>

class GenericDecodeOption
{
};


enum DecoderType : uint8_t
{
	DT_EvercoastVoxel = 0,
	DT_CortoMesh,
	DT_WebpImage,
	DT_Invalid
};

enum DecodeResultType
{
	DRT_EvercoastVoxel = 0,
	DRT_CortoMesh,
	DRT_WebpImage,
	DRT_CortoMesh_WebpImage_Unified,
	DRT_Invalid
};

class GenericDecodeResult
{
public:
	GenericDecodeResult(bool success, double timestamp, int64_t index) : 
		DecodeSuccessful(success),
		frameTimestamp(timestamp),
		frameIndex(index)
	{
	}


	virtual ~GenericDecodeResult() {}

	bool DecodeSuccessful;
	double frameTimestamp;
	int64_t frameIndex;

	virtual DecodeResultType GetType() const
	{
		return DecodeResultType::DRT_Invalid;
	}

	virtual void InvalidateResult()
	{
		DecodeSuccessful = false;
		frameTimestamp = 0;
		frameIndex = -1;
	}

	bool IsValid() const
	{
		return DecodeSuccessful;
	}
};


class IGenericDecoder
{
public:
	virtual DecoderType GetType() const = 0;
	virtual bool DecodeMemoryStream(const uint8_t* stream, size_t stream_size, double timestamp, int64_t frameIndex, GenericDecodeOption* option) = 0;
	virtual std::shared_ptr<GenericDecodeResult> GetResult() = 0;
};