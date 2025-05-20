#pragma once
#include "CoreMinimal.h"
#include "GenericDecoder.h"
#include <vector>
#include <memory>
#include <mutex>

class EVERCOASTPLAYBACK_API WebpDecodeResult : public GenericDecodeResult
{
public:
	WebpDecodeResult(int width, int height, uint8_t bpp);
	WebpDecodeResult(const WebpDecodeResult& rhs);
	virtual ~WebpDecodeResult();

	// for thread safety
	void Lock() const;
	void Unlock() const;

	// Applying result transfer the data ownership
	bool ApplyResult(bool success, double timestamp, int64_t frameIndex, int width, int height, uint8_t bpp, uint8_t* data);

	virtual DecodeResultType GetType() const override
	{
		return DecodeResultType::DRT_WebpImage;
	}

	int GetImageWidth() const
	{
		return Width;
	}

	int GetImageHeight() const
	{
		return Height;
	}

	int Width;
	int Height;
	uint8_t BitPerPixel;
	uint8_t* RawTexelBuffer;

	mutable std::mutex RWLock;
};


class EVERCOASTPLAYBACK_API WebpDecoder : public IGenericDecoder
{
public:
	static std::shared_ptr<WebpDecoder> Create();
	virtual ~WebpDecoder();

	virtual DecoderType GetType() const override;
	virtual bool DecodeMemoryStream(const uint8_t* stream, size_t stream_size, double timestamp, int64_t frameIndex, GenericDecodeOption* option) override;
	virtual std::shared_ptr<GenericDecodeResult> TakeResult() override;

	void SetReceivingResult(std::shared_ptr<WebpDecodeResult> receivingResult)
	{
		result = receivingResult;
	}

	void UnsetReceivingResult()
	{
		result.reset();
	}

private:
	std::shared_ptr<WebpDecodeResult> result;
};