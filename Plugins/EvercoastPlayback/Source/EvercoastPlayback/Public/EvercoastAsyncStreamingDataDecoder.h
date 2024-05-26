#pragma once
#include <memory>
#include "EvercoastStreamingDataDecoder.h"
#include "ec_decoder_compatibility.h"

class EvercoastDecoder;
class FRunnable;
class FRunnableThread;
struct EvercoastLocalDataFrame;
class IEvercoastStreamingDataUploader;

class EVERCOASTPLAYBACK_API EvercoastAsyncStreamingDataDecoder : public IEvercoastStreamingDataDecoder
{
public:
	static constexpr int DEFAULT_BUFFER_COUNT = 30;
	EvercoastAsyncStreamingDataDecoder(std::shared_ptr<IGenericDecoder> base_decoder, std::shared_ptr<IGenericDecoder> aux_decoder);
	virtual ~EvercoastAsyncStreamingDataDecoder();
	// ~Start of IEvercoastStreamingDataDecoder~
	virtual void Receive(double timestamp, int64_t frameIndex, const uint8_t* data, size_t data_size, uint32_t metadata) override;
	virtual std::shared_ptr<GenericDecodeResult> PopResult() override;
	virtual std::shared_ptr<GenericDecodeResult> PeekResult(int offsetFromTop = 0) const override;
	virtual void DisposeResult(std::shared_ptr<GenericDecodeResult> result) override;
	virtual void FlushAndDisposeResults() override;
	virtual void SetRequiresExternalData(bool required) override;
	virtual void ResizeBuffer(uint32_t bufferCount) override;
	// ~End of IEvercoastStreamingDataDecoder~
private:
	void Deinit();
	std::shared_ptr<IGenericDecoder> m_baseDecoder;
	std::shared_ptr<IGenericDecoder> m_auxDecoder;

	// threading
	FRunnable* m_runnable;
	FRunnableThread* m_runnableController;
};
