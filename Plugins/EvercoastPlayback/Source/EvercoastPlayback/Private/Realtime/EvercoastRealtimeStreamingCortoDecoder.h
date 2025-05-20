#pragma once

#include "Realtime/EvercoastRealtimeDataDecoder.h"
#include <memory>

class CortoDecoder;
class WebpDecoder;
class EvercoastPerfCounter;

class EvercoastRealtimeStreamingCortoDecoder : public IEvercoastRealtimeDataDecoder
{
public:
	EvercoastRealtimeStreamingCortoDecoder(std::shared_ptr<EvercoastPerfCounter> perfCounter);
	virtual ~EvercoastRealtimeStreamingCortoDecoder();
	// ~Start of IEvercoastRealtimeDataDecoder~
	virtual void Receive(double timestamp, int64_t frameIndex, const uint8_t* data, size_t data_size, uint32_t metadata) override;
	virtual std::shared_ptr<GenericDecodeResult> PopResult() override;
	virtual std::shared_ptr<GenericDecodeResult> PeekResult(int offsetFromTop = 0) const override;
	virtual void DisposeResult(std::shared_ptr<GenericDecodeResult> result) override;
	virtual void FlushAndDisposeResults() override;
	virtual void ResizeBuffer(uint32_t bufferSize, double halfFrameInterval) override {} // instant buffer
	virtual bool IsGoingToBeFull() const override { return false; }
	// ~End of IEvercoastRealtimeDataDecoder~
private:
	std::shared_ptr<CortoDecoder> m_baseMeshDecoder;
	std::shared_ptr<WebpDecoder> m_baseImageDecoder;

	// threading
	FRunnable* m_runnable;
	FRunnableThread* m_runnableController;
};