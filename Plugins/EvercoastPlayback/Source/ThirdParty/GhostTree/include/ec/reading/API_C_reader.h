// Copyright (C) 2021 Evercoast Inc. All Rights Reserved.

#ifndef GHOSTTREE_API_API_STREAMING_READER_PUBLIC_INCLUDE_EC_READING_API_C_READER_H_
#define GHOSTTREE_API_API_STREAMING_READER_PUBLIC_INCLUDE_EC_READING_API_C_READER_H_

#include <cstdint>

#ifdef __cplusplus
#include "API_macros.hpp"
#else
#define EC_DECODING_API_EXPORT
#endif

#include "API_types.h"

#ifdef __cplusplus
extern "C"
{
#endif
#ifndef EC_DEFINE_HANDLE
#define EC_DEFINE_HANDLE
    typedef uint32_t Handle;
    static const Handle InvalidHandle = 0;
#endif
    EC_READING_API_EXPORT void initialise_reading_api();

    // Interface
    EC_READING_API_EXPORT Handle create_reader_instance();
    EC_READING_API_EXPORT bool release_reader_instance(Handle reader);

    EC_READING_API_EXPORT Config reader_default_config(Handle reader);

    struct ReadDelegate
    {
        void (*on_event)(Handle handle, int32_t event);
        void (*on_playback_info_received)(Handle handle, PlaybackInfo);
        void (*on_meta_data_received)(Handle handle, uint32_t count, const char* keys[], const char* values[]);
        void (*on_channel_received)(Handle handle, uint32_t count, ChannelInfo*);

        void (*on_block_received)(Handle handle, ChannelDataBlock data_block);
        void (*on_block_invalidated)(Handle handle, uint32_t block_id);
        void (*on_last_block)(Handle handle, uint32_t channel_id);

        void (*on_cache_update)(Handle handle, double cached_until);
        void (*on_finished_with_cache_id)(Handle handle, uint32_t cacheId);
        void (*on_free_space_in_cache)(Handle handle, uint32_t cache_id, uint32_t offset, uint32_t size);

        bool (*open_connection)(Handle handle, uint32_t conn_handle, const char* name);
        bool (*read_from_connection)(Handle handle, uint32_t conn_handle, ReadRequest request);
        bool (*cancel_connection_request)(Handle handle, uint32_t conn_handle, uint32_t requestId);
        bool (*read_from_cache)(Handle handle, ReadRequest request);
        bool (*close_connection)(Handle handle, uint32_t conn_handle);
    };

    EC_READING_API_EXPORT bool reader_open(Handle reader, Config config, ReadDelegate delegate);

    EC_READING_API_EXPORT void reader_seek(Handle reader, double time);

    /*
     * During live broadcast poll for the latest the manifest
     * Ideally this should be called periodically based on the playbackInfo.segmentDuration
     * return true if successfully sent for update, false if invalid or already updating
     */
    EC_READING_API_EXPORT bool reader_poll(Handle reader);

    EC_READING_API_EXPORT void reader_enable_channel_representations(Handle reader,
                                                                     uint32_t channel_id,
                                                                     uint32_t representation_count,
                                                                     const uint32_t* representation_ids,
                                                                     bool clear_cache);

    EC_READING_API_EXPORT void reader_finished_with_block(Handle reader, uint32_t block_id);

    EC_READING_API_EXPORT void reader_read_progress(Handle reader, uint32_t request_id, uint32_t amount);
    EC_READING_API_EXPORT void reader_read_complete(Handle reader, uint32_t request_id, uint32_t read_size);
    EC_READING_API_EXPORT void reader_read_failed(Handle reader, uint32_t request_id);

    EC_READING_API_EXPORT void reader_close(Handle reader);

#ifdef __cplusplus
};
#endif

#endif // GHOSTTREE_API_API_STREAMING_READER_PUBLIC_INCLUDE_EC_READING_API_C_READER_H_
