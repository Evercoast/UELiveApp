#include "WebpDecoder.h"
#include "webp/decode.h"

WebpDecodeResult::WebpDecodeResult(int width, int height, uint8_t bpp) :
	GenericDecodeResult(false, 0, 0),
	Width(width), Height(height), BitPerPixel(bpp),
	RawTexelBuffer(nullptr)
{
}

WebpDecodeResult::WebpDecodeResult(const WebpDecodeResult& rhs) :
	GenericDecodeResult(rhs.DecodeSuccessful, rhs.frameTimestamp, rhs.frameIndex),
	Width(rhs.Width), Height(rhs.Height), BitPerPixel(rhs.BitPerPixel),
	RawTexelBuffer(nullptr)
{
	if (DecodeSuccessful)
	{
		size_t size = Width * Height * BitPerPixel / 8;
		if (size > 0)
		{
			RawTexelBuffer = (uint8*)WebPMalloc(size);
			FMemory::Memcpy(RawTexelBuffer, rhs.RawTexelBuffer, size);
		}
	}
}

WebpDecodeResult::~WebpDecodeResult()
{
	if (RawTexelBuffer)
	{
		WebPFree(RawTexelBuffer);
		RawTexelBuffer = nullptr;
	}
}

void WebpDecodeResult::Lock() const
{
	RWLock.lock();
}

void WebpDecodeResult::Unlock() const
{
	RWLock.unlock();
}

bool WebpDecodeResult::ApplyResult(bool success, double timestamp, int64_t frame_index, int width, int height, uint8_t bpp, uint8_t* data)
{
	this->DecodeSuccessful = success;
	this->frameTimestamp = timestamp;
	this->frameIndex = frame_index;

	if (DecodeSuccessful && data)
	{
		this->Width = width;
		this->Height = height;
		this->BitPerPixel = bpp;

		if (RawTexelBuffer)
		{
			WebPFree(RawTexelBuffer);
			RawTexelBuffer = nullptr;
		}

		// ownership transferred here
		RawTexelBuffer = data;
		return true;
	}

	return false;
}

std::shared_ptr<WebpDecoder> WebpDecoder::Create()
{
	return std::make_shared<WebpDecoder>();
}

WebpDecoder::~WebpDecoder()
{
	result.reset();
}

DecoderType WebpDecoder::GetType() const
{
	return DT_WebpImage;
}

bool WebpDecoder::DecodeMemoryStream(const uint8_t* stream, size_t stream_size, double timestamp, int64_t frameIndex, GenericDecodeOption* option)
{
	int width, height;
	if (!WebPGetInfo(stream, stream_size, &width, &height))
	{
		return false;
	}

	result->Lock();

	// We can avoid a memcpy here too
	if (!result->ApplyResult(true, timestamp, frameIndex, width, height, 32, WebPDecodeBGRA(stream, stream_size, &width, &height)))
		return false;

	result->Unlock();
	return true;
}


std::shared_ptr<GenericDecodeResult> WebpDecoder::GetResult()
{
	return result;
}

