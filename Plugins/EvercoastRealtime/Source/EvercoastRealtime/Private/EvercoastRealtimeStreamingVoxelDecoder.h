#pragma once

#include <memory>
#include "EvercoastStreamingDataDecoder.h"

class EvercoastDecoder;
class FRunnable;
class FRunnableThread;
struct EvercoastLocalDataFrame;
class IEvercoastStreamingDataUploader;
class EvercoastPerfCounter;

class EvercoastRealtimeStreamingVoxelDecoder : public IEvercoastStreamingDataDecoder
{
public:
	EvercoastRealtimeStreamingVoxelDecoder(std::shared_ptr<EvercoastPerfCounter> perfCounter);
	virtual ~EvercoastRealtimeStreamingVoxelDecoder();
	// ~Start of IEvercoastStreamingDataDecoder~
	virtual void Receive(double timestamp, int64_t frameIndex, const uint8_t* data, size_t data_size, uint32_t metadata) override;
	virtual std::shared_ptr<GenericDecodeResult> PopResult() override;
	virtual std::shared_ptr<GenericDecodeResult> PeekResult(int offsetFromTop = 0) const override;
	virtual void DisposeResult(std::shared_ptr<GenericDecodeResult> result) override;
	virtual void FlushAndDisposeResults() override;
	virtual void SetRequiresExternalData(bool required) override {}
	virtual void ResizeBuffer(uint32_t bufferSize) override {} // instant buffer
	// ~End of IEvercoastStreamingDataDecoder~
private:
	std::shared_ptr<EvercoastDecoder> m_baseVoxelDecoder;

	// threading
	FRunnable* m_runnable;
	FRunnableThread* m_runnableController;
};