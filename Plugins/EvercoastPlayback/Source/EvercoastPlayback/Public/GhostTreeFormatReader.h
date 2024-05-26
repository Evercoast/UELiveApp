/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-13 01:51:05
* @Last Modified by:   feng_ye
* @Last Modified time: 2023-07-26 13:37:44
*/
#pragma once

#include <memory>
#include <mutex>
#include <queue>
#include <map>
#include "ec_reading_compatibility.h"
#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "HttpModule.h"
#include "Components/AudioComponent.h"
#include "GhostTreeFormatReader.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(EvercoastReaderLog, Log, All);

class IEvercoastStreamingDataDecoder;
class FHttpModule;
class SavedHttpRequest; // we need this silly method to keep IHttpRequest reference alive
class TheReaderDelegate;
class TheValidationDelegate;
class UEvercoastStreamingAudioImportCallback;
class URuntimeAudio;
enum class ERuntimeAudioFactoryResult;

typedef int32_t ECReaderEvent;

extern const TCHAR* GHOSTTREE_DISKCACHE_PREFIX;
extern const TCHAR* GHOSTTREE_DISKCACHE_EXTENSION;

class EvercoastStreamingReaderStatusCallback
{
public:
	virtual void OnWaitingForDataChanged(bool isWaitingForData) = 0;
	virtual void OnInSeekingChanged(bool isInSeeking) = 0;
	virtual void OnPlaybackReadyChanged(bool isPlaybackReady) = 0;
	virtual void OnFatalError(const char* error) = 0;
	virtual void OnWaitingForAudioDataChanged(bool isWaitingForAudioData) = 0;
	virtual void OnWaitingForVideoDataChanged(bool isWaitingForVideoData) = 0;
};

UCLASS()
class UGhostTreeFormatReader : public UObject
{
	GENERATED_BODY()
public:

	static constexpr uint32_t DECODE_META_NONE = 0;
	static constexpr uint32_t DECODE_META_IMAGE_WEBP = 1;

	static UGhostTreeFormatReader* Create(bool inEditor, UAudioComponent* audioComponent, int32 maxCacheSizeInMB);
	virtual ~UGhostTreeFormatReader();

	void SetInitialSeek(float initialSeekTimestamp);
	bool OpenFromLocation(const FString& urlOrFilePath, ReadDelegate readDelegate, std::shared_ptr<IEvercoastStreamingDataDecoder> dataDecoder);
	void Close();
	void SetStatusCallbackRaw(EvercoastStreamingReaderStatusCallback* callback)
	{
		m_statusCallback = callback;
	}
	
	bool RequestFrameOnTimestamp(float timestamp);
	void RequestFrameNext();
	void GetChannelSpatialInfo(FVector& outOrigin, FQuat& outOrientation) const;
	void Tick();
	bool IsWaitingForData() const;
	bool IsWaitingForAudioData() const;
	float GetFrameInterval() const;
	float GetFrameRate() const;
	FString GetExternalPostfix() const;
	bool IsMeshData() const;
	bool IsMeshDataWithNormal() const;
	bool MeshRequiresExternalData() const;
	bool IsInSeeking() const;
	float GetCurrentTimestamp() const;
	float GetDuration() const;
	float GetSeekingTarget() const;
	bool HasFatalError() const;
	bool IsPlaybackReady() const;
	uint32_t GetMainChannelId() const;
	uint32_t GetTextureChannelId() const;
	GTHandle GetRawHandle() const;
	bool IsAudioDataAvailable() const;
	UAudioComponent* GetReceivingAudioComponent() const;
	TArray<uint32_t> GetBitRates() const
	{
		return m_volumetricChannelBitRates;
	}
	void SetBitRateLimit(uint32_t bitRateThreshold)
	{
		m_volumetricChannelBitRateThreshold = bitRateThreshold;
	}

	TArray<uint32_t> GetAvailableFrameRates() const
	{
		return m_availableFrameRates;
	}

	void SetDesiredFrameRate(uint32_t frameRate)
	{
		m_desiredFrameRate = frameRate;
	}

	void SetUsingMemoryCache(bool needMemoryCache)
	{
		m_forceMemoryCache = needMemoryCache;
	}

	void SetPreferExternalVideoData(bool preferVideo)
	{
		m_preferExternalVideoData = preferVideo;
	}
#if WITH_EDITOR
	// only for checking the validity of location. ReadDelegate will have to call 
	bool ValidateLocation(const FString& urlOrFilePath, double timeoutSec); 
#endif
private:

	class IReaderCache
	{
	public:
		virtual bool CopyAdd(uint32_t cache_id, const uint8_t* data, uint32_t size) = 0;
		virtual bool Remove(uint32_t cache_id) = 0;
		virtual const uint8_t* Get(uint32_t cache_id) = 0;
		virtual const uint8_t* GetRange(uint32_t cache_id, uint32_t offset, uint32_t size) = 0;
		virtual void Reset() = 0;
	};

	// A cache mainly caches data to disk, while holding the most recent requested cache in memory.
	class ReaderDiskCache : public IReaderCache
	{
	public:
		ReaderDiskCache();
		virtual ~ReaderDiskCache();
		// Allocate and copy data on the fly. Return true for new cache entry, false for existing entry updated
		virtual bool CopyAdd(uint32_t cache_id, const uint8_t* data, uint32_t size) override;
		virtual bool Remove(uint32_t cache_id) override;
		virtual const uint8_t* Get(uint32_t cache_id) override;
		virtual const uint8_t* GetRange(uint32_t cache_id, uint32_t offset, uint32_t size) override;

		virtual void Reset() override;
	private:
		ReaderDiskCache(const ReaderDiskCache&) = delete;
		ReaderDiskCache& operator=(const ReaderDiskCache&) = delete;

		FArchive* GetCacheFileWriter();
		FArchive* GetCacheFileReader();

		struct DiskCacheRecord
		{
			size_t offset = 0;
			uint32_t size = 0;
		};

		std::map<uint32_t, DiskCacheRecord> m_entries;
		FString m_cacheFileFullpath;
		std::vector<uint8_t> m_currentCache; // just raw array of uint8_t but won't leak
		uint32_t m_currentCacheId;
		std::recursive_mutex m_lock;
	};

	// Memory only cache, to reduce IO pressure
	class ReaderMemoryCache : public IReaderCache
	{
	public:
		ReaderMemoryCache();
		virtual ~ReaderMemoryCache();
		// Allocate and copy data on the fly. Return true for new cache entry, false for existing entry updated
		virtual bool CopyAdd(uint32_t cache_id, const uint8_t* data, uint32_t size) override;
		virtual bool Remove(uint32_t cache_id) override;
		virtual const uint8_t* Get(uint32_t cache_id) override;
		virtual const uint8_t* GetRange(uint32_t cache_id, uint32_t offset, uint32_t size) override;

		virtual void Reset() override;
	private:
		ReaderMemoryCache(const ReaderMemoryCache&) = delete;
		ReaderMemoryCache& operator=(const ReaderMemoryCache&) = delete;

		std::map<uint32_t, uint8_t*> m_entries;
		std::recursive_mutex m_lock;
	};


	// For an UObject you have to separate constructor and init functions like this
	UGhostTreeFormatReader(const FObjectInitializer&);
	void Init(const GTHandle reader_instance, bool inEditor, UAudioComponent* audioComponent, int32 maxCacheSizeInMB);

	void ProcessRequestResults();
	void FinishCurrentBlock();
	static TSharedRef<IHttpRequest, ESPMode::ThreadSafe> NewHttpRequest(const FString& url, uint64_t rangeStart, uint64_t rangeEnd, float timeout);
	void CreateCache();

	// ~Start of ReaderDelegate imlementation~
	void OnReaderEvent(ECReaderEvent event);
	void OnPlaybackInfoReceived(PlaybackInfo playback_info);
	void OnMetaDataReceived(uint32_t count, const char* metadata_keys[], const char* metadata_values[]);
	void OnChannelsReceived(uint32_t count, ChannelInfo* channel_infos);
	void OnNextBlockNotReady(uint32_t channel_id);
	void OnBlockReceived(ChannelDataBlock data_block);
	void OnBlockInvalidated(uint32_t block_id);
	void OnCacheUpdate(double cached_until);
	void OnFinishedWithCacheId(uint32_t cache_id);
	bool OnOpenConnection(uint32_t conn_handle, const char* name);
	bool OnCloseConnection(uint32_t conn_handle);
	bool OnReaderReadFromConnection(uint32_t conn_handle, ReadRequest request);
	// ~End of ReaderDelegate imlementation~

	void OnRuntimeAudioResult(URuntimeAudio* audio, ERuntimeAudioFactoryResult result);

	static bool s_initialised;

	enum OperatingMode
	{
		None = 0,
		FileSystem,
		HTTP
	};

	GTHandle m_instance;
	bool m_inEditor;
	std::string m_dataURL;
	IEvercoastStreamingDataDecoder* m_dataDecoder;

	struct RequestRecord
	{
		uint32_t id;
		uint32_t amount;
	};
	
	std::queue<RequestRecord> m_processedRequestId;
	std::queue<uint32_t> m_failedRequestId;

	OperatingMode m_currMode;
	// Filesystem request
	TSharedPtr<FArchive, ESPMode::NotThreadSafe> m_fileStream;
	// Http request
	std::vector<std::shared_ptr<SavedHttpRequest>> m_savedHttpRequestList;

	std::recursive_mutex m_readerLock;
	bool m_isMesh;
	bool m_isMeshWithNormals;

	uint32_t						m_mainChannelId;
	uint32_t						m_mainChannelSampleRate;
	uint32_t						m_audioChannelId;
	uint32_t						m_currRepresentationId;
	std::map<uint32_t, uint32_t>	m_representationIds; // save frame rate & postfix for each representation(volumetric or mesh only)
	mutable uint32_t				m_cachedLastSampleRate;
	FString							m_currExternalPostfix;
	uint32_t						m_textureChannelId;

	TArray<uint8> m_audioDataCache;
	uint8 m_audioFormat;

	std::shared_ptr<FRunnableThread> m_runtimeAudioDecodeThread;
	UAudioComponent* m_audioComponent;
	float m_audioChannelDuration;
	bool m_audioChannelDataReady;
	bool m_playbackReady;
	bool m_mainChannelDataReady;
	float m_currTimestamp;
	float m_currSeekingTarget;
	float m_outstandingSeekingTarget;
	float m_initSeekingTarget;
	bool m_inSeeking;
	float m_mainChannelDuration;
	bool m_fatalError;

	struct DataBlocks
	{
		ChannelDataBlock MainBlock;
		ChannelDataBlock TextureBlock;
		std::vector<ChannelDataBlock> UnusedBlocks;

		DataBlocks() :
			MainBlock{
				0,
				(uint32_t)-1,
				0,
				0,
				0,
				0.0,
				0.0,
				(uint32_t)-1,
				0
			},
			TextureBlock{
				0,
				(uint32_t)-1,
				0,
				0,
				0,
				0.0,
				0.0,
				(uint32_t)-1,
				0
			}
		{
		}

		void Invalidate(uint32_t blockId);
		void ReleaseAllBlocks(GTHandle reader);
	};
	DataBlocks m_currentBlock;


	Position m_channelOrigin;
	Rotation m_channelOrientation;

	std::shared_ptr<IReaderCache> m_cache;
	EvercoastStreamingReaderStatusCallback* m_statusCallback;
	bool m_validationInProgress;

	TArray<uint32_t> m_volumetricChannelBitRates;
	uint32_t m_volumetricChannelBitRateThreshold;
	TArray<uint32_t> m_availableFrameRates;
	uint32_t m_desiredFrameRate;

	bool m_forceMemoryCache;
	int32 m_maxCacheSizeInMB;
	bool m_preferExternalVideoData;

	friend class TheReaderDelegate;
	friend class TheValidationDelegate;
	friend class UEvercoastStreamingAudioImportCallback;
};
