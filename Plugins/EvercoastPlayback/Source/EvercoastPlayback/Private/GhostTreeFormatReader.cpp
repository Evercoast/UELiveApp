/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-13 01:51:05
* @Last Modified by:   Ye Feng
* @Last Modified time: 2021-12-15 16:00:54
*/
#include "GhostTreeFormatReader.h"
#include "EvercoastStreamingDataDecoder.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpManager.h"
#include "GenericPlatform/GenericPlatformProcess.h"
// For UFS serialisation
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/Archive.h"
#include "Serialization/ArrayReader.h"

#include "Components/AudioComponent.h"
#include <map>
#include <inttypes.h>
#include "ec/reading/API_events.h"
#include "EvercoastPlaybackUtils.h"
#include "RuntimeAudioFactory.h"
#include "RuntimeAudio.h"

#include "zstd.h"
#include "Gaussian/EvercoastGaussianSplatDecoder.h"

DEFINE_LOG_CATEGORY(EvercoastReaderLog);
bool UGhostTreeFormatReader::s_initialised = false;
static std::map<GTHandle, UGhostTreeFormatReader*> s_readerRegistry;

static constexpr int HIGHEST_FRAMERATE = 30;

UGhostTreeFormatReader* find_reader(GTHandle reader_inst)
{
	std::map<GTHandle, UGhostTreeFormatReader*>::iterator it = s_readerRegistry.find(reader_inst);
	check(it != s_readerRegistry.end());
	if (it != s_readerRegistry.end())
	{
		return it->second;
	}

	return nullptr;
}

class TheValidationDelegate
{
public:
	static ReadDelegate get_callbacks_for_c()
	{
		return ReadDelegate
		{
			on_event,
			on_playback_info_received,
			on_meta_data_received,
			on_channels_received,

			//on_next_block_not_ready,
			on_block_received,
			on_block_invalidated,
			on_last_block,

			on_cache_update,
			on_finished_with_cache_id,
			on_free_space_in_cache,

			open_connection,
			read_from_connection,
			cancel_connection_request,
			read_from_cache,
			close_connection
		};
	}

	static void on_event(GTHandle reader_inst, ECReaderEvent event)
	{
		find_reader(reader_inst)->OnReaderEvent(event);
	}

	static void on_playback_info_received(GTHandle reader_inst, PlaybackInfo playback_info)
	{
		find_reader(reader_inst)->OnPlaybackInfoReceived(playback_info);
	}

	static void on_meta_data_received(GTHandle reader_inst, uint32_t count, const char* keys[], const char* values[])
	{
		find_reader(reader_inst)->OnMetaDataReceived(count, keys, values);
	}

	static void on_channels_received(GTHandle reader_inst, uint32_t count, ChannelInfo* channel_infos)
	{
		find_reader(reader_inst)->OnChannelsReceived(count, channel_infos);
	}

	static void on_next_block_not_ready(GTHandle reader_inst, uint32_t channel_id)
	{
		find_reader(reader_inst)->OnNextBlockNotReady(channel_id);
	}

	static void on_block_received(GTHandle reader_inst, ChannelDataBlock data_block)
	{
		find_reader(reader_inst)->OnBlockReceived(data_block);
	}

	static void on_block_invalidated(GTHandle reader_inst, uint32_t block_id)
	{
		find_reader(reader_inst)->OnBlockInvalidated(block_id);
	}

	static void on_last_block(GTHandle reader_inst, uint32_t channel_id)
	{
		find_reader(reader_inst)->OnLastBlock(channel_id);
	}

	static void on_cache_update(GTHandle reader_inst, double cached_until)
	{
		find_reader(reader_inst)->OnCacheUpdate(cached_until);
	}
	static void on_finished_with_cache_id(GTHandle reader_inst, uint32_t cache_id)
	{
		find_reader(reader_inst)->OnFinishedWithCacheId(cache_id);
	}
	static void on_free_space_in_cache(GTHandle handle, uint32_t cache_id, uint32_t offset, uint32_t size)
	{
	}

	static bool open_connection(GTHandle reader_inst, uint32_t conn_handle, const char* name)
	{
		return find_reader(reader_inst)->OnOpenConnection(conn_handle, name);
	}
	static bool read_from_connection(GTHandle reader_inst, uint32_t conn_handle, ReadRequest request)
	{
		return find_reader(reader_inst)->OnReaderReadFromConnection(conn_handle, request);
	}

	static bool cancel_connection_request(GTHandle handle, uint32_t conn_handle, uint32_t requestId)
	{
		return true;
	}

	static bool read_from_cache(GTHandle handle, ReadRequest request)
	{
		return true;
	}

	static bool close_connection(GTHandle reader_inst, uint32_t conn_handle)
	{
		return find_reader(reader_inst)->OnCloseConnection(conn_handle);
	}
};


class SavedHttpRequest
{
public:
	SavedHttpRequest(TSharedRef<IHttpRequest, ESPMode::ThreadSafe> httpRequestRef) : m_requestRef(httpRequestRef), m_finished(false)
	{
	}

	virtual ~SavedHttpRequest()
	{
		// Unbind all lambdas before shutting down the request
		m_requestRef->OnProcessRequestComplete().Unbind();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
		m_requestRef->OnRequestProgress64().Unbind();
#else
		m_requestRef->OnRequestProgress().Unbind();
#endif
		m_requestRef->OnRequestWillRetry().Unbind();
		m_requestRef->OnHeaderReceived().Unbind();
		m_requestRef->CancelRequest();
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Get() const
	{
		return m_requestRef;
	}

	operator TSharedRef<IHttpRequest, ESPMode::ThreadSafe>() const {
		return m_requestRef;
	}

	void MarkAsFinished()
	{
		m_finished = true;
	}

	bool IsFinished() const
	{
		return m_finished;
	}
private:
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> m_requestRef;
	bool m_finished;
};


//////////////////////////////////////////////////////////////////////////////////////////
// ReaderDiskCache
//
const TCHAR* GHOSTTREE_DISKCACHE_PREFIX = TEXT("EC_Cache_");
const TCHAR* GHOSTTREE_DISKCACHE_EXTENSION = TEXT(".bin");

UGhostTreeFormatReader::ReaderDiskCache::ReaderDiskCache() :
	m_currentCacheId(-1)
{
	m_cacheFileFullpath = FPaths::CreateTempFilename(FGenericPlatformMisc::GamePersistentDownloadDir(), GHOSTTREE_DISKCACHE_PREFIX, GHOSTTREE_DISKCACHE_EXTENSION);
}


UGhostTreeFormatReader::ReaderDiskCache::~ReaderDiskCache()
{
	Reset();
}

void UGhostTreeFormatReader::ReaderDiskCache::Reset()
{
	IFileManager::Get().Delete(*m_cacheFileFullpath);

	m_entries.clear();
	m_currentCache.clear();
	m_currentCacheId = -1;
}

FArchive* UGhostTreeFormatReader::ReaderDiskCache::GetCacheFileWriter()
{
	IFileManager& fileManager = IFileManager::Get();
	FArchive* Archive = fileManager.CreateFileWriter(*m_cacheFileFullpath, FILEWRITE_Append);
	if (!Archive)
	{
		UE_LOG(EvercoastReaderLog, Error, TEXT("Cannot write cache file '%s'."), *m_cacheFileFullpath);
		return nullptr;
	}

	return Archive;
}


FArchive* UGhostTreeFormatReader::ReaderDiskCache::GetCacheFileReader()
{
	IFileManager& fileManager = IFileManager::Get();
	if (fileManager.FileExists(*m_cacheFileFullpath))
	{
		FArchive* Archive = fileManager.CreateFileReader(*m_cacheFileFullpath, FILEREAD_NoFail | FILEREAD_Silent);
		if (!Archive)
		{
			UE_LOG(EvercoastReaderLog, Error, TEXT("Cannot read cache file '%s'."), *m_cacheFileFullpath);
			return nullptr;
		}

		return Archive;
	}

	UE_LOG(EvercoastReaderLog, Error, TEXT("Cache file '%s' does not exist."), *m_cacheFileFullpath);
	return nullptr;

	
}

bool UGhostTreeFormatReader::ReaderDiskCache::CopyAdd(uint32_t cache_id, const uint8_t* data, uint32_t size)
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	if (m_entries.find(cache_id) == m_entries.end())
	{
		FArchive* Writer = GetCacheFileWriter();
		size_t offset = Writer->TotalSize();
		Writer->Serialize((void*)data, size);
		Writer->Close();

		m_entries.insert(std::pair<uint32_t, DiskCacheRecord>(cache_id, { offset, size }));
		return true;
	}
	else
	{
		// existing entry, update the cache
		FArchive* Writer = GetCacheFileWriter();
		size_t offset = Writer->TotalSize();
		Writer->Serialize((void*)data, size);
		Writer->Close();

		m_entries[cache_id] = { offset, size };

		return false;
	}
}

bool UGhostTreeFormatReader::ReaderDiskCache::Remove(uint32_t cache_id)
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	auto it = m_entries.find(cache_id);
	if (it != m_entries.end())
	{
		// We don't do anything to the disk cache file as it's grow-only
		m_entries.erase(it);

		// if happens to be the current cache
		if (cache_id == m_currentCacheId)
		{
			m_currentCacheId = -1;
			m_currentCache.clear();
		}
		return true;
	}

	return false;
}

const uint8_t* UGhostTreeFormatReader::ReaderDiskCache::Get(uint32_t cache_id)
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);

	if (cache_id != m_currentCacheId)
	{
		auto it = m_entries.find(cache_id);
		if (it != m_entries.end())
		{
			FArchive* Reader = GetCacheFileReader();
			if (Reader)
			{
				Reader->Seek(it->second.offset);

				m_currentCacheId = cache_id;
				m_currentCache.resize(it->second.size);

				Reader->Serialize(m_currentCache.data(), it->second.size);
				Reader->Close();
				return m_currentCache.data();
			}
		}

		return nullptr;
	}
	else
	{
		if (m_currentCache.size() > 0)
			return m_currentCache.data();
		else
			return nullptr;
	}
}

const uint8_t* UGhostTreeFormatReader::ReaderDiskCache::GetRange(uint32_t cache_id, uint32_t offset, uint32_t size)
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	if (cache_id != m_currentCacheId)
	{
		auto it = m_entries.find(cache_id);
		if (it != m_entries.end())
		{
			FArchive* Reader = GetCacheFileReader();
			if (Reader)
			{
				Reader->Seek(it->second.offset);

				m_currentCacheId = cache_id;
				m_currentCache.resize(it->second.size);

				Reader->Serialize(m_currentCache.data(), it->second.size);
				Reader->Close();
				return m_currentCache.data() + offset;
			}
		}

		return nullptr;
	}
	else
	{
		if (m_currentCache.size() > 0)
			return m_currentCache.data() + offset;
		else
			return nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// UGhostTreeFormatReader::ReaderMemoryCache
UGhostTreeFormatReader::ReaderMemoryCache::ReaderMemoryCache()
{
}


UGhostTreeFormatReader::ReaderMemoryCache::~ReaderMemoryCache()
{
	Reset();
}

void UGhostTreeFormatReader::ReaderMemoryCache::Reset()
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	for (auto it = m_entries.begin(); it != m_entries.end(); ++it)
	{
		delete[] it->second;
	}

	m_entries.clear();
}

bool UGhostTreeFormatReader::ReaderMemoryCache::CopyAdd(uint32_t cache_id, const uint8_t* data, uint32_t size)
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	auto find_it = m_entries.find(cache_id);
	if (find_it == m_entries.end())
	{
		uint8_t* copied_data = new uint8_t[size];
		memcpy(copied_data, data, size);

		m_entries.insert(std::pair<uint32_t, uint8*>(cache_id, copied_data));
		return true;

	}
	else
	{
		delete[] find_it->second;

		uint8_t* copied_data = new uint8_t[size];
		memcpy(copied_data, data, size);

		find_it->second = copied_data;

		return false;
	}
}

bool UGhostTreeFormatReader::ReaderMemoryCache::Remove(uint32_t cache_id)
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	auto it = m_entries.find(cache_id);
	if (it != m_entries.end())
	{
		delete[] it->second;
		m_entries.erase(it);
		return true;
	}

	return false;
}

const uint8_t* UGhostTreeFormatReader::ReaderMemoryCache::Get(uint32_t cache_id)
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	auto it = m_entries.find(cache_id);
	if (it != m_entries.end())
	{
		return it->second;
	}

	return nullptr;
}

const uint8_t* UGhostTreeFormatReader::ReaderMemoryCache::GetRange(uint32_t cache_id, uint32_t offset, uint32_t size)
{
	std::lock_guard<std::recursive_mutex> guard(m_lock);
	auto it = m_entries.find(cache_id);
	if (it != m_entries.end())
	{
		uint8_t* buf = it->second;
		return buf + offset;
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////
// UGhostTreeFormatReader
UGhostTreeFormatReader* UGhostTreeFormatReader::Create(bool inEditor, UAudioComponent* audioComponent, int32 maxCacheSizeInMB, UObject* Outter)
{
	if (!UGhostTreeFormatReader::s_initialised)
	{
		initialise_reading_api();
		UGhostTreeFormatReader::s_initialised = true;
	}
	
	UGhostTreeFormatReader* reader = NewObject<UGhostTreeFormatReader>(Outter);
	reader->Init(create_reader_instance(), inEditor, audioComponent, maxCacheSizeInMB);
	return reader;
}

UGhostTreeFormatReader::UGhostTreeFormatReader(const FObjectInitializer&) :
	m_instance(InvalidHandle),
	m_inEditor(false),
	m_currMode(OperatingMode::None),
	m_isMesh(false),
	m_isMeshWithNormals(false),
	m_mainChannelId(-1),
	m_mainChannelSampleRate(0),
	m_audioChannelId(-1),
	m_currRepresentationId(-1),
	m_cachedLastSampleRate(15),
	m_textureChannelId(-1),
	m_audioFormat(RuntimeAudioFormat::UNKNOWN),
	m_audioComponent(nullptr),
	m_audioChannelDuration(0),
	m_audioChannelDataReady(false),
	m_playbackReady(false),
	m_mainChannelDataReady(false),
	m_currTimestamp(0),
	m_currSeekingTarget(0),
	m_initSeekingTarget(0),
	m_inSeeking(false),
	m_mainChannelDuration(0),
	m_fatalError(false),
	m_channelOrigin{ 0, 0, 0 },
	m_channelOrientation{ 0, 0, 0, 1 },
	m_statusCallback(nullptr),
	m_validationInProgress(false),
	m_volumetricChannelBitRateThreshold((uint32_t)-1),
	m_desiredFrameRate(HIGHEST_FRAMERATE),
	m_forceMemoryCache(false),
	m_maxCacheSizeInMB(1024),
	m_preferExternalVideoData(false)
{
}

void UGhostTreeFormatReader::Init(const GTHandle reader_instance, bool inEditor, UAudioComponent* audioComponent, int32 maxCacheSizeInMB)
{
	m_instance = reader_instance;
	check(m_instance != InvalidHandle);

	m_inEditor = inEditor;
	m_audioComponent = audioComponent;
	m_maxCacheSizeInMB = maxCacheSizeInMB;

	s_readerRegistry.insert(std::make_pair(reader_instance, this));
}

UGhostTreeFormatReader::~UGhostTreeFormatReader()
{
	ProcessRequestResults();
	// finish the last block available before closing
	FinishPendingBlocks();
	ProcessRequestResults();

	m_dataDecoder = nullptr;
	release_reader_instance(m_instance);
	s_readerRegistry.erase(m_instance);

	m_instance = InvalidHandle;

	if (m_cache)
	{
		m_cache->Reset();
		m_cache = nullptr;
	}

	m_savedHttpRequestList.clear();

	if (m_fileStream)
	{
		m_fileStream->Close();
		m_fileStream.Reset();
	}

	m_currMode = OperatingMode::None;
	m_statusCallback = nullptr;

	if (m_runtimeAudioDecodeThread)
	{
		m_runtimeAudioDecodeThread->Kill(true);
		m_runtimeAudioDecodeThread.reset();
	}

	m_audioComponent = nullptr;
}

void UGhostTreeFormatReader::OnReaderEvent(ECReaderEvent event)
{
	FString eventName("UnknownEvent");
	switch (event)
	{
	case EC_STREAMING_EVENT_PLAYBACK_READY:
		eventName = "PlaybackReady";
		UE_LOG(EvercoastReaderLog, Verbose, TEXT("Event: %s=%d"), *eventName, (int)event);
		if (!m_playbackReady)
		{
			m_playbackReady = true;
			if (m_statusCallback)
			{
				m_statusCallback->OnPlaybackReadyChanged(m_playbackReady);
			}
		}
		return;

	case EC_STREAMING_EVENT_INSUFFICIENT_CACHE:
		eventName = "InsufficientCache";
		UE_LOG(EvercoastReaderLog, Verbose, TEXT("Event: %s=%d"), *eventName, (int)event);
		return;

	case EC_STREAMING_EVENT_ERROR_UNKNOWN:
		eventName = "Error";
		m_fatalError = true;
		if (m_statusCallback)
		{
			m_statusCallback->OnFatalError("Unknown Error");
		}
		break;
	case EC_STREAMING_EVENT_INVALID_ECV:
		eventName = "InvalidECV";
		m_fatalError = true;
		if (m_statusCallback)
		{
			m_statusCallback->OnFatalError("Invalid ECV Error");
		}
		break;
	case EC_STREAMING_EVENT_INVALID_SEEK_TIME:
		eventName = "InvalidSeekTime";
		m_currSeekingTarget = m_currTimestamp;
		if (m_inSeeking)
		{
			m_inSeeking = false;
			if (m_statusCallback)
			{
				m_statusCallback->OnInSeekingChanged(m_inSeeking);
			}
		}

		// we need to set playback ready flag, as the seek operation had previously de-set it
		// shouldn't be worried about playback wasn't ready as the invalid seek time event
		// only happens when the seeking is undergoing, thus playback data should already be available.
		if (!m_playbackReady)
		{
			m_playbackReady = true;
			if (m_statusCallback)
			{
				m_statusCallback->OnPlaybackReadyChanged(m_playbackReady);
			}
		}
		
		if (!m_mainChannelDataReady)
		{
			m_mainChannelDataReady = true; // there's no such timestamp, just mark data available
			if (m_statusCallback)
			{
				m_statusCallback->OnWaitingForDataChanged(!m_mainChannelDataReady);
			}
		}
		break;
	case EC_STREAMING_EVENT_NO_CHANNEL_ENABLED:
		eventName = "NoChannelEnabled";
		m_fatalError = true;
		if (m_statusCallback)
		{
			m_statusCallback->OnFatalError("No Channel Enabled Error");
		}
		break;
	case EC_STREAMING_EVENT_READ_COMPLETE_DURING_REQUEST:
		eventName = "ReadCompleteDuringRequest";
		m_fatalError = true;
		if (m_statusCallback)
		{
			m_statusCallback->OnFatalError("ReadComplete Called During Request Error");
		}
		break;
	case EC_STREAMING_EVENT_READ_FAILED:
		eventName = "ReadFailed";
		m_fatalError = true;
		if (m_statusCallback)
		{
			m_statusCallback->OnFatalError("Read Failed Error");
		}
		break;
	case EC_STREAMING_EVENT_READ_INVALID_RESPONSE:
		eventName = "ReadInvalidResponse";
		m_fatalError = true;
		if (m_statusCallback)
		{
			m_statusCallback->OnFatalError("Read Invalid Response Error");
		}
		break;
	case EC_STREAMING_EVENT_SEEK_UNAVAILABLE:
		eventName = "SeekUnavailable";
		// Could be internal error, or the reader could be just be busy 
		UE_LOG(EvercoastReaderLog, Warning, TEXT("Seek Unavailable for timestamp: %.2f"), m_currSeekingTarget);
		break;
	case EC_STREAMING_EVENT_UNKNOWN_CHANNEL:
		eventName = "UnknownChannel";
		break;
	case EC_STREAMING_EVENT_UNKNOWN_READ_REQUEST_ID:
		eventName = "UnknownRequestId";
		break;
	case EC_STREAMING_EVENT_UNKNOWN_REPRESENTATION:
		eventName = "UnknownRepresentation";
		break;
	}
	UE_LOG(EvercoastReaderLog, Warning, TEXT("Event: %s=%d"), *eventName, (int)event);
}

bool UGhostTreeFormatReader::OnOpenConnection(uint32_t conn_handle, const char* name)
{
	UE_LOG(EvercoastReaderLog, Log, TEXT("Open connection: %s"), *FString(m_dataURL.c_str()));

	
	if (m_dataURL.rfind("http://", 0) == 0 || m_dataURL.rfind("https://", 0) == 0)
	{
		m_currMode = OperatingMode::HTTP;
		return true;
	}
	else
	{
		FString pathURL = FString(m_dataURL.c_str());
		FString dummyPath = FString(pathURL);
		bool isRelativePath = FPaths::IsRelative(pathURL);
		if (isRelativePath)
		{
			// Conver to absolute path
			pathURL = FPaths::Combine(FPaths::ProjectDir(), pathURL);
			pathURL = FPaths::ConvertRelativePathToFull(pathURL);
		}

		UE_LOG(EvercoastReaderLog, Log, TEXT("Open local converted path: %s"), *pathURL);
	
		FArchive* ar = IFileManager::Get().CreateFileReader(*pathURL);
		if (ar)
		{
			m_fileStream = MakeShareable(ar);
			if (!m_fileStream->GetError() && m_fileStream->IsLoading())
			{
				m_currMode = OperatingMode::FileSystem;
				return true;
			}

			UE_LOG(EvercoastReaderLog, Error, TEXT("Open with error: %s"), *pathURL);
			m_fileStream->ClearError();
		}
		else
		{
			UE_LOG(EvercoastReaderLog, Error, TEXT("Open failed: %s"), *pathURL);
		}
		
	}
	return false;
}

bool UGhostTreeFormatReader::OnCloseConnection(uint32_t conn_handle)
{
	UE_LOG(EvercoastReaderLog, Log, TEXT("Close connection: %s"), *FString(m_dataURL.c_str()));

	if (m_currMode == OperatingMode::HTTP)
	{
		m_savedHttpRequestList.clear();
	}
	else if (m_currMode == OperatingMode::FileSystem)
	{
		m_fileStream->Close();
		m_fileStream.Reset();
	}
	else
	{
		check(!m_fileStream && m_savedHttpRequestList.size() == 0);
	}

	if (m_playbackReady)
	{
		m_playbackReady = false;
		if (m_statusCallback)
		{
			m_statusCallback->OnPlaybackReadyChanged(m_playbackReady);
		}
	}
	m_currMode = OperatingMode::None;
	return true;
}


bool UGhostTreeFormatReader::OnReaderReadFromConnection(uint32_t conn_handle, ReadRequest readRequest)
{
	
	UE_LOG(EvercoastReaderLog, Verbose, TEXT("Read from %s request id: %d"), *FString(m_dataURL.c_str()), readRequest.request_id);

	if (m_currMode == OperatingMode::HTTP)
	{
		if (readRequest.size == 0)
		{
			std::lock_guard<std::recursive_mutex> guard(m_readerLock);
			UE_LOG(EvercoastReaderLog, Error, TEXT("HTTP request %d has size of 0. No proceeding further."), readRequest.request_id);
			m_failedRequestId.push(readRequest.request_id);

			// force close connection. it seems we have no means to recover from it
			m_fatalError = true;
			if (m_statusCallback)
			{
				m_statusCallback->OnFatalError("HTTP Request Zero Size Error");
			}
			return false;
		}
		uint64_t rangeStart = readRequest.offset;
		uint64_t rangeEnd = readRequest.offset + readRequest.size - 1;
		const float timeout = m_inEditor ? 0.0f : 5.0f;
		auto httpRequest = NewHttpRequest(FString(m_dataURL.c_str()), rangeStart, rangeEnd, timeout);
		auto savedHttpRequest = std::make_shared<SavedHttpRequest>(httpRequest);
		m_savedHttpRequestList.emplace_back(savedHttpRequest);

		// Need to use raw pointer to prevent lambda from keeping the shared_ptr SavedHttpRequest from being destoryed when
		// container cleared.
		SavedHttpRequest* pSavedHttpRequest = savedHttpRequest.get();

		UGhostTreeFormatReader* reader = this;
		httpRequest->OnProcessRequestComplete().BindLambda([readRequest, reader, pSavedHttpRequest](auto httpReq, auto httpResp, bool succeeded) {

			if (!reader->IsValidLowLevel())
			{
				return;
			}

			bool successRead = false;
			if (succeeded && httpResp->GetResponseCode() >= 200 && httpResp->GetResponseCode() < 300)
			{
				if (readRequest.buffer != nullptr)
				{
					// copy response content to readRequest.buffer, no caching, reader api needs some validation before giving cacheable data
					memcpy(readRequest.buffer, httpResp->GetContent().GetData(), std::min(readRequest.size, (uint32_t)httpResp->GetContentLength()));
					successRead = true;
				}
				else
				{
					UE_LOG(EvercoastReaderLog, Verbose, TEXT("HTTP got cache id: %d"), readRequest.cache_id);
					successRead = reader->m_cache->CopyAdd(readRequest.cache_id, httpResp->GetContent().GetData(), std::min(readRequest.size, (uint32_t)httpResp->GetContentLength()));
				}
				
			}

			if (successRead)
			{
				UE_LOG(EvercoastReaderLog, Verbose, TEXT("HTTP request successful id %d"), readRequest.request_id);
				std::lock_guard<std::recursive_mutex> guard(reader->m_readerLock);
				reader->m_processedRequestId.push(
					{
						readRequest.request_id,
						readRequest.size
					});
			}
			else
			{
				UE_LOG(EvercoastReaderLog, Warning, TEXT("HTTP request failed id %d"), readRequest.request_id);
				std::lock_guard<std::recursive_mutex> guard(reader->m_readerLock);
				reader->m_failedRequestId.push(readRequest.request_id);
			}

			pSavedHttpRequest->MarkAsFinished();
		});

		return httpRequest->ProcessRequest();
	}
	else if (m_currMode == OperatingMode::FileSystem)
	{
		bool successRead = false;

		m_fileStream->Seek(readRequest.offset);
		if (!m_fileStream->GetError())
		{
			
			if (readRequest.buffer != nullptr)
			{
				m_fileStream->Serialize((char*)readRequest.buffer, readRequest.size);
				successRead = !m_fileStream->GetError();
			}
			else
			{
				UE_LOG(EvercoastReaderLog, Verbose, TEXT("File cache id: %d"), readRequest.cache_id);

				uint8_t* buffer = new uint8_t[readRequest.size];
				m_fileStream->Serialize(buffer, readRequest.size);
				if (!m_fileStream->GetError())
				{
					successRead = true;
					m_cache->CopyAdd(readRequest.cache_id, buffer, readRequest.size);
				}
				else
				{
					UE_LOG(EvercoastReaderLog, Error, TEXT("File stream read error, cache id = %d, size = %d"), readRequest.cache_id, readRequest.size);
				}

				delete[] buffer;

			}
		}

		if (successRead)
		{
			std::lock_guard<std::recursive_mutex> guard(m_readerLock);
			UE_LOG(EvercoastReaderLog, Verbose, TEXT("File request successful id %d"), readRequest.request_id);
			m_processedRequestId.push(
				{
					readRequest.request_id,
					readRequest.size
				});
			return true;
		}
		else
		{
			std::lock_guard<std::recursive_mutex> guard(m_readerLock);
			UE_LOG(EvercoastReaderLog, Warning, TEXT("File request failed id %d"), readRequest.request_id);
			m_failedRequestId.push(readRequest.request_id);
			m_fileStream->ClearError();
		}
	}
	else
	{
		UE_LOG(EvercoastReaderLog, Warning, TEXT("Unknown operating mode: %d"), (int)m_currMode);
	}
		
	return false;
}

void UGhostTreeFormatReader::OnPlaybackInfoReceived(PlaybackInfo playback_info)
{
	if (playback_info.isLive)
	{
		UE_LOG(EvercoastReaderLog, Log, TEXT("PlaybackInfo: Live Segment Duration=%f"), playback_info.segmentDuration);
	}
	else
	{
		UE_LOG(EvercoastReaderLog, Log, TEXT("PlaybackInfo: Duration=%f"), playback_info.duration);
	}
}

void UGhostTreeFormatReader::OnMetaDataReceived(uint32_t count, const char* metadata_keys[], const char* metadata_values[])
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const char* key = metadata_keys[i];
		const char* val = metadata_values[i];
		UE_LOG(EvercoastReaderLog, Log, TEXT("Metadata: %s = %s"), *FString(key), *FString(val));
	}
}

void UGhostTreeFormatReader::OnChannelsReceived(uint32_t count, ChannelInfo* channel_infos)
{
	bool mainChannelSelected = false;
	bool audioChannelSelected = false;
	bool textureChannelSelected = false;
	m_volumetricChannelBitRates.Empty();
	m_availableFrameRates.Empty();

	// Clean the runtime audio if provided
	if (m_audioComponent)
		m_audioComponent->SetSound(nullptr);
	m_audioChannelDuration = 0;

	// first loop to deal with main volumetric/geom data
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto& info = channel_infos[i];


		FString desc(info.description);
		UE_LOG(EvercoastReaderLog, Log, TEXT("ChannelInfo: channel_id=%d, description=%s"), info.channel_id, *desc);

		if (desc.StartsWith("volumetric") || desc.StartsWith("mesh"))
		{
			// do we need to save AABB?
			const auto& aabb = info.aabb;

			m_channelOrigin = info.origin;
			m_channelOrientation = info.rotation;

			bool meshWithNormals = false;

			if (desc.StartsWith("mesh/corto/n"))
			{
				meshWithNormals = true;
			}

			// Either it's not mesh format(volumetric), or the mesh-with-normal channel has NOT been registered
			// This ensures mesh with normal channel is preferred over mesh without normal
			if (!m_isMeshWithNormals || !m_isMesh)
			{
				m_mainChannelId = info.channel_id;
				m_mainChannelDuration = info.duration;
			}

			uint32_t sampleRate = 0;
			uint32_t selectedRepresentationId = 0;
			uint32_t representationCount = info.representation_count;
			for (uint32_t j = 0; j < representationCount; ++j)
			{
				if (info.representations[j].bit_rate < m_volumetricChannelBitRateThreshold)
				{
					// Shall we use the highest frame rate
					if (m_desiredFrameRate == 0 && info.representations[j].sample_rate > sampleRate)
					{
						// Use this representation
						selectedRepresentationId = info.representations[j].representation_id;
						// Save its frame rate
						m_representationIds[selectedRepresentationId] = info.representations[j].sample_rate;
						// bookkeeping sample rate
						sampleRate = info.representations[j].sample_rate;
					}
					// Or we match the desired frame rate
					else
					if (sampleRate == 0 || info.representations[j].sample_rate == m_desiredFrameRate)
					{
						// Use this representation
						selectedRepresentationId = info.representations[j].representation_id;
						// Save its frame rate
						m_representationIds[selectedRepresentationId] = info.representations[j].sample_rate;
						// bookkeeping sample rate
						sampleRate = info.representations[j].sample_rate;
					}
				}

				m_availableFrameRates.AddUnique(info.representations[j].sample_rate);
				m_volumetricChannelBitRates.AddUnique(info.representations[j].bit_rate);
			}

			if (selectedRepresentationId == 0 || sampleRate == 0)
			{
				mainChannelSelected = false;
			}
			else
			{
				std::vector<uint32_t> representationIdInUse;
				representationIdInUse.emplace_back(selectedRepresentationId);
				reader_enable_channel_representations(m_instance, info.channel_id, representationIdInUse.size(), representationIdInUse.data(), true);

				m_mainChannelSampleRate = sampleRate;

				mainChannelSelected = true;

				if (meshWithNormals)
					m_isMeshWithNormals = true;
			}
		}
	}

	if (!mainChannelSelected)
	{
		UE_LOG(EvercoastReaderLog, Error, TEXT("Unable to select main channel's representation. The reader will not continue to work."));
		return;
	}

	// second & third loop to deal with encoded texture data, if needed
	if (m_isMesh)
	{
		bool hasSuitableWebpData = false;
		bool hasSuitableVideoData = false;

		// Find a proper texture channel
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto& info = channel_infos[i];
			FString desc(info.description);

			if (desc.StartsWith("texture/webp"))
			{
				uint32_t desiredFrameRate = m_mainChannelSampleRate;
				uint32_t representationCount = info.representation_count;
				for (uint32_t j = 0; j < representationCount; ++j)
				{
					if (info.representations[j].sample_rate == desiredFrameRate)
					{
						hasSuitableWebpData = true;
						break;
					}
				}
			}
			else if (desc.StartsWith("texture/mp4"))
			{
				uint32_t desiredFrameRate = m_mainChannelSampleRate;
				uint32_t representationCount = info.representation_count;
				for (uint32_t j = 0; j < representationCount; ++j)
				{
					if (info.representations[j].sample_rate == desiredFrameRate)
					{
						hasSuitableVideoData = true;
						break;
					}
				}
			}
		}

		if (!hasSuitableWebpData && !hasSuitableVideoData)
		{
			UE_LOG(EvercoastReaderLog, Warning, TEXT("Unable to find texture channel for mesh data for: %s. The ecm version is out-of-date. Force using default video filename."), *FString(m_dataURL.c_str()));
			m_currExternalPostfix = FString(TEXT(".mp4"));
			textureChannelSelected = true;
		}
		else
		{
			bool enableWebpData = false;
			bool enableVideoData = false;
			if (hasSuitableWebpData && !hasSuitableVideoData)
			{
				// only look for webp
				enableWebpData = true;
			}
			else if (!hasSuitableWebpData && hasSuitableVideoData)
			{
				// only look for video
				enableVideoData = true;
			}
			else
			{
				// based on preference
				enableVideoData = m_preferExternalVideoData;
				enableWebpData = !m_preferExternalVideoData;
			}

			for (uint32_t i = 0; i < count; ++i)
			{
				const auto& info = channel_infos[i];
				FString desc(info.description);

				if (enableWebpData && desc.StartsWith("texture/webp"))
				{
					uint32_t desiredFrameRate = m_mainChannelSampleRate;
					uint32_t representationCount = info.representation_count;
					std::vector<uint32_t> representationIdInUse;

					for (uint32_t j = 0; j < representationCount; ++j)
					{
						if (info.representations[j].sample_rate == desiredFrameRate)
						{
							representationIdInUse.emplace_back(info.representations[j].representation_id);
						}
					}

					if (representationIdInUse.empty())
					{
						UE_LOG(EvercoastReaderLog, Warning, TEXT("Unable to find proper encoded texture representation with sample rate %d."), desiredFrameRate);
					}
					else
					{
						reader_enable_channel_representations(m_instance, info.channel_id, representationIdInUse.size(), representationIdInUse.data(), true);
						m_textureChannelId = info.channel_id;
						textureChannelSelected = true;
					}

				}

				if (enableVideoData && desc.StartsWith("texture/mp4"))
				{
					uint32_t desiredFrameRate = m_mainChannelSampleRate;
					uint32_t representationCount = info.representation_count;
					for (uint32_t j = 0; j < representationCount; ++j)
					{
						if (info.representations[j].sample_rate == desiredFrameRate)
						{
							m_currExternalPostfix = info.representations[j].external_postfix;
						}
					}

					if (m_currExternalPostfix.IsEmpty())
					{
						UE_LOG(EvercoastReaderLog, Warning, TEXT("Unable to find external file prefix."));
						m_currExternalPostfix = FString(TEXT(".mp4"));
					}

					textureChannelSelected = true;
				}
			}
		}
	}

	if (m_isMesh && !textureChannelSelected)
	{
		UE_LOG(EvercoastReaderLog, Warning, TEXT("Unable to find texture channel for mesh data based on preference for: %s. Force using default video filename."), *FString(m_dataURL.c_str()));
		m_currExternalPostfix = FString(TEXT(".mp4"));
		textureChannelSelected = true;
	}

	// Final loop deal with audio data
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto& info = channel_infos[i];
		FString desc(info.description);

		if (desc.StartsWith("audio/"))
		{
			std::vector<uint32_t> representationIdInUse;
			uint32_t representationCount = info.representation_count;
			for (uint32_t j = 0; j < representationCount; ++j)
			{
				// Use any representations
				representationIdInUse.emplace_back(info.representations[j].representation_id);
			}

			if (desc.StartsWith("audio/mpeg") || desc.StartsWith("audio/wav"))
			{
				if (info.duration > 0)
				{
					m_audioChannelId = info.channel_id;

					if (desc.StartsWith("audio/wav"))
					{
						m_audioFormat = (uint8)RuntimeAudioFormat::WAV;
					}
					else
					{
						m_audioFormat = (uint8)RuntimeAudioFormat::MP3;
					}
					if (m_audioChannelDataReady)
					{
						m_audioChannelDataReady = false;
						if (m_statusCallback)
						{
							m_statusCallback->OnWaitingForAudioDataChanged(!m_audioChannelDataReady);
						}
					}

					reader_enable_channel_representations(m_instance, info.channel_id, representationIdInUse.size(), representationIdInUse.data(), true);
					audioChannelSelected = true;
				}
				else
				{
					UE_LOG(EvercoastReaderLog, Warning, TEXT("%s channel %d has zero duration. This audio channel cannot be used."), *desc, info.channel_id);
				}
			}
			else
			{
				UE_LOG(EvercoastReaderLog, Warning, TEXT("Audio type %s is not yet supported."), *desc);
			}
		}
	}

	if (mainChannelSelected)
	{
		// resize decoder buffer to match video decoder's setting, 1 sec before cursor and 1 sec after cursor
		if (m_dataDecoder)
		{
			// feed with 2 x frame rate for both 1s before cursor and 1s after cursor's frame
			// and query width is 1/2 frame interval
			m_dataDecoder->ResizeBuffer(m_mainChannelSampleRate * 2, 0.5 / m_mainChannelSampleRate);

			
		}
	
		// initiate reading
		reader_seek(m_instance, m_initSeekingTarget);
	}

	// if no audio selected, callback not waiting for audio data 
	if (!audioChannelSelected)
	{
		m_audioChannelDataReady = true;
		if (m_statusCallback)
		{
			m_statusCallback->OnWaitingForAudioDataChanged(!m_audioChannelDataReady);
		}
	}
}

void UGhostTreeFormatReader::OnNextBlockNotReady(uint32_t channel_id)
{
	if (channel_id == m_mainChannelId)
	{
		UE_LOG(EvercoastReaderLog, Warning, TEXT("Next volumetric block not ready: channel: %d"), channel_id);
		if (m_mainChannelDataReady)
		{
			m_mainChannelDataReady = false;
			if (m_statusCallback)
			{
				m_statusCallback->OnWaitingForDataChanged(!m_mainChannelDataReady);
			}
		}
	}
	else if (channel_id == m_audioChannelId)
	{
		UE_LOG(EvercoastReaderLog, Warning, TEXT("Next audio block not ready: channel: %d"), channel_id);
	}
}

void UGhostTreeFormatReader::OnBlockReceived(ChannelDataBlock data_block)
{
	UE_LOG(EvercoastReaderLog, Verbose, TEXT("Block Received: block_id: %d, channel_id: %d"), data_block.block_id, data_block.channel_id);

	uint32_t data_size = data_block.size;
	const uint8_t* data = m_cache->GetRange(data_block.cache_id, data_block.offset, data_block.size);
	if (!data)
	{
		UE_LOG(EvercoastReaderLog, Error, TEXT("No cache found by cache_id %d"), data_block.cache_id);
		return;
	}

	if (data_block.channel_id == m_mainChannelId)
	{
		
		// send data and data_size to decoder and get the result
		if (m_dataDecoder)
		{
			UE_LOG(EvercoastReaderLog, Verbose, TEXT("Decoding voxels at time: %.2f block: %d channel: %d repr: %d"),
				data_block.timestamp, data_block.block_id, data_block.channel_id, data_block.representation_id);

            // Extract frame number from block data
            uint32_t frameNumber = data[data_size - 1] << 24 | data[data_size - 2] << 16 | data[data_size - 3] << 8 | data[data_size - 4];
			m_dataDecoder->Receive(data_block.timestamp, GetFrameIndex(data_block.timestamp, GetFrameRate()), data, data_size, DECODE_META_NONE);
			
			if (!m_dataDecoder->IsGoingToBeFull())
			{
				std::lock_guard<std::recursive_mutex> guard(m_pendingReleaseBlocksLock);
				// put request next frame into pending list, to be processed in Tick()
				m_pendingDataBlocksToRelease.push_back(data_block);

				UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Continue request more GEOM blocks after: %.2f block id: %d"), data_block.timestamp, data_block.block_id);
			}
			else
			{
				UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Data decoder cache is full, no more requesting new GEOM blocks. Saved block: %.2f block id: %d"), data_block.timestamp, data_block.block_id);
				m_lastBlockStub.MainBlock = data_block;
			}
		}
		else
		{

			UE_LOG(EvercoastReaderLog, Warning, TEXT("Main channel: %d has not been assigned decoder! Throw away block: %d"), data_block.channel_id, data_block.block_id);

			/*
			// Try feed raw bytes to zstd
			auto decompressedSize = ZSTD_getFrameContentSize(data, data_size);
			if (decompressedSize == ZSTD_CONTENTSIZE_ERROR || decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN)
			{
				UE_LOG(EvercoastReaderLog, Error, TEXT("Getting frame compressed metadata error. Data: %P, Size: %d"), data, data_size);
			}
			else
			{
				uint8_t* rawBytes = new uint8_t[decompressedSize];
				auto actualDecompressedSize = ZSTD_decompress(rawBytes, decompressedSize, data, data_size);

				// rawBytes should contain an ECSPZ frame
				ECSpzHeader* header = (ECSpzHeader*)rawBytes;
				UE_LOG(EvercoastReaderLog, Log, TEXT("SPZ header magic: 0x%08x version: %d pointCount: %d frameNumber: %d"), header->magic, header->version, header->pointCount, header->frameNumber);
				delete[] rawBytes;
			}
			*/

			// Not supported codec/or in validation process, (no data decoder) throw the block to m_pendingDataBlocksToRelease
			std::lock_guard<std::recursive_mutex> guard(m_pendingReleaseBlocksLock);

			m_pendingDataBlocksToRelease.push_back(data_block);
		}

		m_currTimestamp = (float)data_block.timestamp;

		m_currRepresentationId = data_block.representation_id;

		// check if the seeking target meets
		if (m_inSeeking)
		{
			if (fabs(m_currSeekingTarget - data_block.timestamp) <= GetFrameInterval())
			{
				m_inSeeking = false;
				if (m_statusCallback)
				{
					m_statusCallback->OnInSeekingChanged(m_inSeeking);
				}
				auto it = m_seekCallbacks.find(data_block.timestamp);
				if ( it != m_seekCallbacks.end())
				{
					(*it).second();
					m_seekCallbacks.erase(it);
				}
				UE_LOG(EvercoastReaderLog, Verbose, TEXT("Seek to %.3f - %.3f finished"), m_currSeekingTarget, data_block.timestamp);
			}
		}

		if (!m_mainChannelDataReady)
		{
			m_mainChannelDataReady = true;
			if (m_statusCallback)
			{
				m_statusCallback->OnWaitingForDataChanged(!m_mainChannelDataReady);
			}
		}
	}
	else if (data_block.channel_id == m_textureChannelId)
	{
		if (m_dataDecoder)
		{
			UE_LOG(EvercoastReaderLog, Verbose, TEXT("Decoding texture at time: %.2f block: %d channel: %d repr: %d"),
				data_block.timestamp, data_block.block_id, data_block.channel_id, data_block.representation_id);

			m_dataDecoder->Receive(data_block.timestamp, GetFrameIndex(data_block.timestamp, GetFrameRate()), data, data_size, DECODE_META_IMAGE_WEBP);

			if (!m_dataDecoder->IsGoingToBeFull())
			{
				std::lock_guard<std::recursive_mutex> guard(m_pendingReleaseBlocksLock);
				// put request next frame into pending list, to be processed in Tick()
				m_pendingDataBlocksToRelease.push_back(data_block);

				UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Continue request more TEX blocks after: %.2f, block id: %d"), data_block.timestamp, data_block.block_id);
			}
			else
			{
				UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Data decoder cache is full, no more requesting new TEX blocks. Saved block: %.2f, block_id: %d"), data_block.timestamp, data_block.block_id);
				m_lastBlockStub.TextureBlock = data_block;
			}
		}
	}
	else if (data_block.channel_id == m_audioChannelId)
	{
		UE_LOG(EvercoastReaderLog, Log, TEXT("Fetched audio data: 0x%p, size %d"), data, data_size);

		if (m_validationInProgress)
		{
			// no sound validation, pass silently
		}
		else 
		if (!m_audioComponent)
		{
			UE_LOG(EvercoastReaderLog, Warning, 
				TEXT("No audio component found, even though this url: %s does provide audio data. This actor will not play audio unless you add an AudioComponent to it."),
				*FString(m_dataURL.c_str()));

			// Still need it to be functional, so mark audio as non-waiting
			m_audioChannelDataReady = true;
			if (m_statusCallback)
			{
				m_statusCallback->OnWaitingForAudioDataChanged(!m_audioChannelDataReady);
			}
		}
		else
		{
			// process the whole audio data from the block, generating the play-able sound wave
			//
			// Try only import once
			if (m_currSeekingTarget == 0 || !m_audioComponent->Sound)
			{
				if (!IsGarbageCollecting())
				{
					// Stop playing before importing, so that Play() won't get accidentally called on worker thread
					m_audioComponent->Stop();
					m_audioChannelDataReady = false;
					if (m_statusCallback)
					{
						m_statusCallback->OnWaitingForAudioDataChanged(!m_audioChannelDataReady);
					}

					// At least we need to kill the last importing if any
					if (m_runtimeAudioDecodeThread)
					{
						UE_LOG(EvercoastReaderLog, Verbose, TEXT("Destroy previous decode thread: %s"), *m_runtimeAudioDecodeThread->GetThreadName());
						
						m_runtimeAudioDecodeThread->Kill(true);
						m_runtimeAudioDecodeThread.reset();
					}

					TArray<uint8_t> audioDataCache(data, data_size);
					FOnRuntimeAudioFactoryResult callback;
					callback.AddUObject(this, &UGhostTreeFormatReader::OnRuntimeAudioResult);
					this->AddToRoot(); // prevent accidentally deletion
					m_runtimeAudioDecodeThread = FRuntimeAudioFactory::CreateRuntimeAudioFromBuffer(audioDataCache, callback);
				}
			}
			else
			{
				//UE_LOG(EvercoastReaderLog, Log, TEXT("Rewind Playback Time: %.2f"), m_currSeekingTarget);
				auto soundWave = m_audioComponent->Sound;
				if (soundWave->IsA<URuntimeAudio>())
				{
					URuntimeAudio* oldSoundWave = static_cast<URuntimeAudio*>(m_audioComponent->Sound);
					oldSoundWave->SeekToTime(m_currSeekingTarget);
				}
			}
			
		}

		UE_LOG(EvercoastReaderLog, Log, TEXT("Finish audio block: block_id: %d"), data_block.block_id);
		reader_finished_with_block(m_instance, data_block.block_id);
	}
	else
	{
		UE_LOG(EvercoastReaderLog, Warning, TEXT("Block(not processed): block_id: %d"), data_block.block_id);
		// Avoid calling reader_finished_with_block() otherwise in some scenarios like Sequencer it will
		// cause reading subsequent block going to be end up here unprocessed, then stack overflow
		//reader_finished_with_block(m_instance, data_block.block_id);

		std::lock_guard<std::recursive_mutex> guard(m_pendingReleaseBlocksLock);
		// put request next frame into pending list, to be processed in Tick()
		m_pendingDataBlocksToRelease.push_back(data_block);
	}
}


void UGhostTreeFormatReader::FinishPendingBlocks()
{
	std::lock_guard<std::recursive_mutex> guard(m_pendingReleaseBlocksLock);

	// Since reader_finished_with_block() will callback OnBlockReceived, here we need index based iteration to handle
	// new items
	for(size_t i = 0; i < m_pendingDataBlocksToRelease.size(); ++i)
	{
		ChannelDataBlock& block = m_pendingDataBlocksToRelease[i];
		UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Finish pending block id: %d"), block.block_id);
		reader_finished_with_block(m_instance, block.block_id);
	}
	m_pendingDataBlocksToRelease.clear();
}

bool UGhostTreeFormatReader::RequestFrameOnTimestamp(float timestamp)
{
	if (m_currMode != OperatingMode::None)
	{
		UE_LOG(EvercoastReaderLog, Verbose, TEXT("Request seek to %.2f"), timestamp);

		FinishPendingBlocks();

		m_currSeekingTarget = timestamp;

		// Always set to the real timestamp to 1 sec before the requrested, this is aligned with the frame caching algorithm
		float timestampIncludingPrecache = timestamp - 1.0f;
		if (timestampIncludingPrecache < 0)
		{
			timestampIncludingPrecache = 0;
		}

		if (!m_inSeeking)
		{
			m_inSeeking = true;
			if (m_statusCallback)
			{
				m_statusCallback->OnInSeekingChanged(m_inSeeking);
			}
		}

		if (m_mainChannelDataReady)
		{
			m_mainChannelDataReady = false;
			if (m_statusCallback)
			{
				m_statusCallback->OnWaitingForDataChanged(!m_mainChannelDataReady);
			}
		}

		if (m_playbackReady)
		{
			m_playbackReady = false;
			if (m_statusCallback)
			{
				m_statusCallback->OnPlaybackReadyChanged(m_playbackReady);
			}
		}

		// call seek() at the last, as it in turn calls on_event
		reader_seek(m_instance, (double)timestampIncludingPrecache);
		return true;
	}

	return false;
}


bool UGhostTreeFormatReader::RequestFrameOnTimestamp(float timestamp, const std::function<void()>& callback)
{
	m_seekCallbacks[timestamp] = callback;
	return RequestFrameOnTimestamp(timestamp);
}

void UGhostTreeFormatReader::ContinueRequest()
{
	if (m_dataDecoder && !m_dataDecoder->IsGoingToBeFull())
	{
		std::lock_guard<std::recursive_mutex> guard(m_pendingReleaseBlocksLock);
		// put request next frame into pending list, to be processed in Tick()
		if (m_lastBlockStub.MainBlock.block_id != (uint32_t)-1)
		{
			UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Continue request from Main Block: %d timestamp: %.2f"), m_lastBlockStub.MainBlock.block_id, m_lastBlockStub.MainBlock.timestamp);
			m_pendingDataBlocksToRelease.push_back(m_lastBlockStub.MainBlock);
			m_lastBlockStub.ClearMainBlock();
		}
		if (m_lastBlockStub.TextureBlock.block_id != (uint32_t)-1)
		{
			UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Continue request from Tex Block: %d timestamp: %.2f"), m_lastBlockStub.TextureBlock.block_id, m_lastBlockStub.TextureBlock.timestamp);
			m_pendingDataBlocksToRelease.push_back(m_lastBlockStub.TextureBlock);
			m_lastBlockStub.ClearTextureBlock();
		}
	}
}

void UGhostTreeFormatReader::OnBlockInvalidated(uint32_t block_id)
{
	UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Block invalidated: block_id: %d"), block_id);
	
	//m_currentBlock.Invalidate(block_id);
	std::lock_guard<std::recursive_mutex> guard(m_pendingReleaseBlocksLock);

	m_pendingDataBlocksToRelease.erase(std::remove_if(m_pendingDataBlocksToRelease.begin(), m_pendingDataBlocksToRelease.end(),
		[block_id](ChannelDataBlock block) {
			return block.block_id == block_id;
		}), m_pendingDataBlocksToRelease.end());


	m_currRepresentationId = -1;
}

void UGhostTreeFormatReader::OnLastBlock(uint32_t channel_id)
{
	// FIXME: not getting called, maybe only http streaming only??
	if (channel_id == m_mainChannelId)
	{
		// TODO: if universally called, here can be the place for seek back to 0 for looping
	}
}

void UGhostTreeFormatReader::OnCacheUpdate(double cached_until)
{
	UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Cache updated till: %f"), cached_until);
}

void UGhostTreeFormatReader::OnFinishedWithCacheId(uint32_t cache_id)
{
	if (m_cache->Remove(cache_id))
	{
		UE_LOG(EvercoastReaderLog, Verbose, TEXT("Cache id removed: %d"), cache_id);
	}
	else
	{
		UE_LOG(EvercoastReaderLog, Warning, TEXT("Unable to remove cache id: %d"), cache_id);
	}
}

bool UGhostTreeFormatReader::OpenFromLocation(const FString& urlOrFilePath, ReadDelegate readDelegate, std::shared_ptr<IEvercoastStreamingDataDecoder> dataDecoder)
{
	Config config = reader_default_config(m_instance);

	config.max_cache_memory_size = 1024 * 1024 * m_maxCacheSizeInMB;
	config.progressive_download = false;

	m_dataURL = TCHAR_TO_ANSI(*urlOrFilePath);
	m_dataDecoder = dataDecoder.get();
	if (urlOrFilePath.EndsWith(".ecm"))
	{
		m_isMesh = true;
		m_isMeshWithNormals = false; // defaultly we don't have normals
	}
	m_mainChannelId = -1;
	m_audioChannelId = -1;
	m_currRepresentationId = -1;
	m_representationIds.clear();
	m_currExternalPostfix.Empty();
	m_textureChannelId = -1;

	CreateCache();

	return reader_open(m_instance, config, readDelegate);
}

void UGhostTreeFormatReader::CreateCache()
{
	if (m_cache)
	{
		m_cache->Reset();
	}

	if (m_forceMemoryCache)
	{
		m_cache = std::make_shared<ReaderMemoryCache>();
	}
	else
	{
		m_cache = std::make_shared<ReaderDiskCache>();
	}
}

void UGhostTreeFormatReader::Close()
{
	// Wait for audio opening thread to be finished, not likely but happens!
	if (m_runtimeAudioDecodeThread)
	{
		UE_LOG(EvercoastReaderLog, Verbose, TEXT("Wait for decode thread on Close: %s"), *m_runtimeAudioDecodeThread->GetThreadName());
		m_runtimeAudioDecodeThread->Kill(true);
		m_runtimeAudioDecodeThread.reset();
	}

	m_audioComponent = nullptr;

	ProcessRequestResults();
	// finish the last block available before closing
	FinishPendingBlocks();

	ProcessRequestResults();
	reader_close(m_instance);

	if (m_currMode == OperatingMode::HTTP)
	{
		m_savedHttpRequestList.clear();
	}

	if (m_currMode == OperatingMode::FileSystem)
	{
		m_fileStream->Close();
		m_fileStream.Reset();
	}

	if (m_cache)
	{
		m_cache->Reset();
		m_cache = nullptr;
	}
	m_currMode = OperatingMode::None;

	if (m_inSeeking)
	{
		m_inSeeking = false;
		if (m_statusCallback)
		{
			m_statusCallback->OnInSeekingChanged(m_inSeeking);
		}
	}
	if (m_playbackReady)
	{
		m_playbackReady = false;
		if (m_statusCallback)
		{
			m_statusCallback->OnPlaybackReadyChanged(m_playbackReady);
		}
	}
	if (m_mainChannelDataReady)
	{
		m_mainChannelDataReady = false;
		if (m_statusCallback)
		{
			m_statusCallback->OnWaitingForDataChanged(!m_mainChannelDataReady);
		}
	}
	m_currTimestamp = 0;
	m_currSeekingTarget = 0;
	m_mainChannelId = -1;
	m_audioChannelId = -1;
	m_currRepresentationId = -1;
	m_representationIds.clear();
	m_currExternalPostfix.Empty();
	m_textureChannelId = -1;

	m_fatalError = false;

}

// The public face for ProcessRequestResults() just looks less awful
void UGhostTreeFormatReader::Tick()
{
	ProcessRequestResults();

	// remove finished http requests from the list
	m_savedHttpRequestList.erase(
		std::remove_if(m_savedHttpRequestList.begin(), m_savedHttpRequestList.end(), [](std::shared_ptr<SavedHttpRequest> request) {
			return request->IsFinished();
			}), 
		m_savedHttpRequestList.end());

	// process all pending blocks
	FinishPendingBlocks();
}


void UGhostTreeFormatReader::ProcessRequestResults()
{
	std::lock_guard<std::recursive_mutex> guard(m_readerLock);

	while (!m_processedRequestId.empty())
	{
		auto& record = m_processedRequestId.front();
		reader_read_complete(m_instance, record.id, record.amount);
		m_processedRequestId.pop();
	}

	while (!m_failedRequestId.empty())
	{
		uint32_t id = m_failedRequestId.front();
		reader_read_failed(m_instance, id);
		m_failedRequestId.pop();
	}
}

void UGhostTreeFormatReader::GetChannelSpatialInfo(FVector& outOrigin, FQuat& outOrientation) const
{
	outOrigin = FVector(m_channelOrigin.x, m_channelOrientation.z, m_channelOrientation.y);
	outOrientation = FQuat(m_channelOrientation.x, m_channelOrientation.z, m_channelOrientation.y, m_channelOrientation.w);
}

bool UGhostTreeFormatReader::IsWaitingForData() const
{
	return !m_mainChannelDataReady;
}

bool UGhostTreeFormatReader::IsWaitingForAudioData() const
{
	return !m_audioChannelDataReady;
}

float UGhostTreeFormatReader::GetFrameInterval() const
{
	return 1.0f / GetFrameRate();
}

uint32_t UGhostTreeFormatReader::GetFrameRate() const
{
	if (m_currRepresentationId >= 0 && m_representationIds.find(m_currRepresentationId) != m_representationIds.cend())
	{
		m_cachedLastSampleRate = m_representationIds.at(m_currRepresentationId);
		return m_cachedLastSampleRate;
	}

	return m_cachedLastSampleRate; // return a legacy default framerate, but we should not get here.
}

FString UGhostTreeFormatReader::GetExternalPostfix() const
{
	if (m_currExternalPostfix.IsEmpty())
		return FString(TEXT(".mp4"));

	return m_currExternalPostfix;
}

bool UGhostTreeFormatReader::IsMeshData() const
{
	return m_isMesh;
}

bool UGhostTreeFormatReader::IsMeshDataWithNormal() const
{
	return m_isMesh && m_isMeshWithNormals;
}

bool UGhostTreeFormatReader::MeshRequiresExternalData() const
{
	return m_isMesh && m_textureChannelId == -1;
}

bool UGhostTreeFormatReader::IsInSeeking() const
{
	return m_inSeeking;
}

float UGhostTreeFormatReader::GetCurrentTimestamp() const
{
	return m_currTimestamp;
}

float UGhostTreeFormatReader::GetDuration() const
{
	if (m_audioChannelDuration > 0)
	{
		return std::min(m_mainChannelDuration, m_audioChannelDuration);
	}
	else
	{
		return m_mainChannelDuration;
	}
}

float UGhostTreeFormatReader::GetSeekingTarget() const
{
	return m_currSeekingTarget;
}

bool UGhostTreeFormatReader::IsPlaybackReady() const
{
	return m_playbackReady;
}

uint32_t UGhostTreeFormatReader::GetMainChannelId() const
{
	return m_mainChannelId;
}

uint32_t UGhostTreeFormatReader::GetTextureChannelId() const
{
	return m_textureChannelId;
}

GTHandle UGhostTreeFormatReader::GetRawHandle() const
{
	return m_instance;
}

bool UGhostTreeFormatReader::HasFatalError() const
{
	return m_fatalError;
}

bool UGhostTreeFormatReader::IsAudioDataAvailable() const
{
	return m_audioComponent && m_audioChannelId != -1;
}

UAudioComponent* UGhostTreeFormatReader::GetReceivingAudioComponent() const
{
	return m_audioComponent;
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UGhostTreeFormatReader::NewHttpRequest(const FString& url, uint64_t rangeStart, uint64_t rangeEnd, float timeout)
{
	char buf[256];
	std::snprintf(buf, 256, "bytes=%" PRIu64 "-%" PRIu64, rangeStart, rangeEnd);
	auto httpRequest = FHttpModule::Get().CreateRequest();
	httpRequest->SetURL(url);
	httpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
	httpRequest->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	httpRequest->SetHeader(TEXT("Accepts"), TEXT("application/octet-stream"));
	httpRequest->SetHeader(TEXT("Origin"), TEXT("evercoast.com"));
	if (timeout > 0)
		httpRequest->SetTimeout(timeout);
	httpRequest->SetHeader(TEXT("Range"), FString(buf));
	httpRequest->SetVerb(TEXT("GET"));

	return httpRequest;
}

// This callback will be forced to run on game thread
void UGhostTreeFormatReader::OnRuntimeAudioResult(URuntimeAudio* newRuntimeAudio, ERuntimeAudioFactoryResult result)
{
	// Need to do it in game thread as it might trigger audio component to play, which only allowed to called on game thread
	if (!m_audioComponent)
	{
		// This check is necessary as the MUTLICAST_DELEGATE.Broadcast() method can be very much delayed on main thread
		// When loading maps, the GT reader object might get constructed then destructed quickly, leaving the callback
		// calling on an closed, or even destroyed GT reader object.
		return;
	}

	if (result == ERuntimeAudioFactoryResult::Succeeded)
	{
		m_audioComponent->SetSound(newRuntimeAudio);
		m_audioChannelDuration = newRuntimeAudio->GetDuration();
	}
	else
	{
		UE_LOG(EvercoastReaderLog, Warning, TEXT("Failed to read/decode audio data."));
		m_audioComponent->SetSound(nullptr);
		m_audioChannelDuration = 0;
	}

	// Anyway we inform the audio is ready
	if (!m_audioChannelDataReady)
	{
		m_audioChannelDataReady = true;
		if (m_statusCallback)
		{
			m_statusCallback->OnWaitingForAudioDataChanged(!m_audioChannelDataReady);
		}
	}

	// Clean up from GC
	RemoveFromRoot();
}

void UGhostTreeFormatReader::SetInitialSeek(float startTimestamp)
{
	m_initSeekingTarget = startTimestamp;
}

#if WITH_EDITOR
bool UGhostTreeFormatReader::ValidateLocation(const FString& urlOrFilePath, double timeoutSec)
{
	Config config = reader_default_config(m_instance);

	config.max_cache_memory_size = 1024 * 1024 * m_maxCacheSizeInMB;
	config.progressive_download = false;

	m_dataURL = TCHAR_TO_ANSI(*urlOrFilePath);
	m_dataDecoder = nullptr;
	if (urlOrFilePath.EndsWith(".ecm"))
	{
		m_isMesh = true;
		m_isMeshWithNormals = false; // defaultly we don't have normals
	}

	CreateCache();

	m_validationInProgress = true;

	bool result = false;
	bool canOpen = reader_open(m_instance, config, TheValidationDelegate::get_callbacks_for_c());
	if (canOpen)
	{
		Tick();
		FinishPendingBlocks();
		auto startTime = FDateTime::Now();
		auto& httpManager = FHttpModule::Get().GetHttpManager();
		while (true)
		{
			Tick();
			FPlatformProcess::Sleep(0.1f);
			// sync method, have to manually tick http manager, need an async way and showing an dialog..
			httpManager.Tick(0.1f);

			auto timespan = FDateTime::Now() - startTime;
			if (IsPlaybackReady())
			{
				UE_LOG(EvercoastReaderLog, Log, TEXT("Validation took %.2f seconds"), timespan.GetTotalSeconds());
				result = true;
				break;
			}
			if (HasFatalError())
			{
				result = false;
				break;
			}

			
			if (timespan.GetTotalSeconds() >= timeoutSec)
			{
				// open url timeout
				result = false;
				break;
			}
		}
	}
	else
	{
		result = false;
	}

	m_validationInProgress = false;
	return result;
}
#endif
