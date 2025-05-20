#pragma once
#include <memory>
#include <mutex>
#include <vector>
#include "EvercoastStreamingDataDecoder.h"
#include "ec_decoder_compatibility.h"

class EvercoastVoxelDecoder;
class FRunnable;
class FRunnableThread;
struct EvercoastLocalDataFrame;
class IEvercoastStreamingDataUploader;


class FEvercoastGenericDecodeThread : public FRunnable
{
public:

	virtual bool HasNewEntry() const = 0;
	virtual size_t GetNewEntryCount() const = 0;
	virtual void AddEntry(double timestamp, int64_t frameIndex, const uint8_t* data, size_t dataSize, uint32_t metadata) = 0;
	virtual int GetPotentialResultCount() const = 0;
	virtual void FlushAndDisposeResults() = 0;

};

class EVERCOASTPLAYBACK_API EvercoastAsyncStreamingDataDecoder : public IEvercoastStreamingDataDecoder
{
public:
	class ResultCache
	{
	public:
		ResultCache(int initialBufferCount);
		virtual ~ResultCache()
		{
			Dispose();
		}

		// The usual add result to cache. The result should be lightweight(e.g. EvercoastVoxelDecodeResult)
		void Add(std::shared_ptr<GenericDecodeResult> result);
		// When results are heavy (e.g. CortoDecodeResult & WebpDecodeResult), it better be allocated inside ResultCache to be reused later
		// Returns null when such object isn't being created
		std::shared_ptr<GenericDecodeResult> Prealloc();
		// When the preallocation returns null, it's the caller's job to supply a prefabricated result object
		void FillLastPrealloc(std::shared_ptr<GenericDecodeResult> newCache);
		// Manipulate preallocation need to freeze the cache as a whole
		void Lock();
		// Manipulate preallocation need to freeze the cache as a whole
		void Unlock();
		std::shared_ptr<GenericDecodeResult> Query(double timestamp, double halfFrameInterval);
		bool Trim(double medianTimestamp, double halfFrameInterval, int halfCacheWidth);
		void Dispose();
		void DisposeAndReinit();
		void Resize(uint32_t bufferCount);
		int Size() const;
		bool IsFull() const;
		bool IsEmpty() const;

		bool IsGoingToBeFull(int futureResultCount) const;
		bool IsBeyond(double timestamp, double halfFrameInterval) const;

	private:
		std::shared_ptr<GenericDecodeResult>* m_resultArray; // ring of cached results

		// when (end + 1) % count == start, the ring is full
		int m_resultStartIdx;
		int m_resultEndIdx;
		int m_bufferCount;

		mutable std::recursive_mutex m_mutex;
	};

	// Middle man between ResultCache and specific FEvercoastGenericDecodeThread, sort the delivered result in timestamp ascending order
	// before feeding to ResultCache
	class ResultPresorter
	{
	public:

		ResultPresorter(ResultCache& resultCache, double frameInterval);

		void Add(std::shared_ptr<GenericDecodeResult> result);

		void Dispose();
		void DisposeAndReinit();

		void CleanupPresortedResults();

	private:
		void CheckContinuityAndFeed();
		void ForceDeliver(std::shared_ptr<GenericDecodeResult> result);

		std::vector<std::shared_ptr<GenericDecodeResult>> m_presortedResults;
		double m_frameInterval;
		double m_lastDeliveredTimestamp;
		ResultCache& m_resultCache;

		mutable std::recursive_mutex m_mutex;
	};



	static constexpr int DEFAULT_BUFFER_COUNT = 30;
	EvercoastAsyncStreamingDataDecoder(DecoderType decoderType);
	virtual ~EvercoastAsyncStreamingDataDecoder();
	// ~Start of IEvercoastStreamingDataDecoder~
	virtual void Receive(double timestamp, int64_t frameIndex, const uint8_t* data, size_t data_size, uint32_t metadata) override;
	virtual std::shared_ptr<GenericDecodeResult> QueryResult(double timestamp) override;
	virtual std::shared_ptr<GenericDecodeResult> QueryResultAfterTimestamp(double afterTimestamp) override;
	virtual bool IsTimestampBeyondCache(double timestamp) override;
	virtual bool TrimCache(double medianTimestamp) override;
	virtual void FlushAndDisposeResults() override;
	virtual void SetRequiresExternalData(bool required) override;
	virtual void ResizeBuffer(uint32_t bufferCount, double halfFrameInterval) override;
	virtual bool IsGoingToBeFull() const override;
	// ~End of IEvercoastStreamingDataDecoder~

	
private:
	void Init(double frameInterval, int maxThreadCount);
	void Deinit();
	FEvercoastGenericDecodeThread* FindLeastJobWorker();

	std::shared_ptr<IGenericDecoder> m_baseDecoder;
	std::shared_ptr<IGenericDecoder> m_auxDecoder;

	// threading
	std::vector<FEvercoastGenericDecodeThread*> m_decodeWorkers;
	std::vector<FRunnableThread*> m_decodeWorkerControllers;

	ResultCache m_resultCache;
	ResultPresorter* m_resultPresorter;
	uint32_t m_halfCacheWidth;
	double m_halfFrameInterval;

	DecoderType m_decoderType;
};
