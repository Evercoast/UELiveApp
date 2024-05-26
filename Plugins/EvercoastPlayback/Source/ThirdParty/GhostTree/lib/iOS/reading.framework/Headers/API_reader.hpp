// Copyright (C) 2020 Evercoast Inc. All Rights Reserved.

#ifndef GHOST_TREE_PUBLIC_INCLUDE_EC_GHOST_TREE_API_STREAMING_READER_HPP_
#define GHOST_TREE_PUBLIC_INCLUDE_EC_GHOST_TREE_API_STREAMING_READER_HPP_

#include <ec/reading/API_events.h>
#include <ec/reading/API_macros.hpp>

#include <memory>
#include <string_view>
#include <vector>

#include <cstdint>

namespace ec
{
    namespace streaming
    {
        namespace reading
        {

#include <ec/reading/API_types.h>

            enum class Format
            {
                VOD_SingleFile
            };

            enum class Event
            {
                // valid playback events
                PlaybackReady = EC_STREAMING_EVENT_PLAYBACK_READY,
                InsufficientCache = EC_STREAMING_EVENT_INSUFFICIENT_CACHE,

                // playback error events
                Error = EC_STREAMING_EVENT_ERROR_UNKNOWN,
                NoChannelEnabled = EC_STREAMING_EVENT_NO_CHANNEL_ENABLED,
                SeekUnavailable = EC_STREAMING_EVENT_SEEK_UNAVAILABLE,
                InvalidSeekTime = EC_STREAMING_EVENT_INVALID_SEEK_TIME,
                UnknownChannel = EC_STREAMING_EVENT_UNKNOWN_CHANNEL,
                UnknownRepresentation = EC_STREAMING_EVENT_UNKNOWN_REPRESENTATION,
                InvalidECV = EC_STREAMING_EVENT_INVALID_ECV,
                InvalidOperation = EC_STREAMING_EVENT_INVALID_OPERATION,
                InvalidMethod = EC_STREAMING_EVENT_INVALID_METHOD,

                // read error events
                ReadFailed = EC_STREAMING_EVENT_READ_FAILED,
                UnknownReadRequestId = EC_STREAMING_EVENT_UNKNOWN_READ_REQUEST_ID,
                ReadCompleteDuringRequest = EC_STREAMING_EVENT_READ_COMPLETE_DURING_REQUEST,
                ReadInvalidResponse = EC_STREAMING_EVENT_READ_INVALID_RESPONSE
            };

            class ReadDelegate
            {
            public:
                ReadDelegate() = default;
                virtual ~ReadDelegate() = default;

                virtual void on_event(Event event) = 0;

                virtual void on_playback_info_received(PlaybackInfo playback_info) = 0;
                virtual void on_meta_data_received(const std::vector<std::pair<std::string_view, std::string_view>>& meta_data) = 0;
                virtual void on_channels_received(std::vector<ChannelInfo>&& channel_infos) = 0;

                virtual void on_block_received(ChannelDataBlock data_block) = 0;
                virtual void on_block_invalidated(uint32_t block_id) = 0;
                virtual void on_last_block(uint32_t channel_id) = 0;

                virtual void on_cache_update(double cached_until) = 0;
                virtual void on_finished_with_cache_id(uint32_t cache_id) = 0;
                virtual void on_free_space_in_cache(uint32_t cache_id, uint32_t offset, uint32_t size) = 0;

                virtual bool open_connection(uint32_t conn_handle, const char* name) = 0;
                virtual bool read_from_connection(uint32_t conn_handle, ReadRequest request) = 0;
                virtual bool cancel_connection_request(uint32_t conn_handle, uint32_t request_id) = 0;
                virtual bool read_from_cache(ReadRequest request) = 0;
                virtual bool close_connection(uint32_t conn_handle) = 0;
            };

            class Interface
            {
            protected:
                Interface() = default;
                virtual ~Interface() = default;

            public:
                [[nodiscard]] virtual Config default_config() = 0;

                virtual bool open(Config config, std::unique_ptr<ReadDelegate>&& readDelegate) = 0;

                virtual void seek(double time) = 0;

                /*
                 * During live broadcast poll for the latest the manifest
                 * Ideally this should be called periodically based on the playbackInfo.segmentDuration
                 * return true if successfully sent for update, false if invalid or already updating
                 */
                virtual bool poll() = 0;

                virtual void enable_channel_representations(uint32_t channel_id, std::vector<uint32_t>&& representation_ids, bool clear_cache) = 0;

                virtual void finished_with_block(uint32_t block_id) = 0;

                virtual void read_progress(uint32_t request_id, uint32_t amount) = 0;
                virtual void read_complete(uint32_t request_id, uint32_t read_size) = 0;
                virtual void read_failed(uint32_t request_id) = 0;

                virtual void close() = 0;
            };

            EC_READING_API_EXPORT std::shared_ptr<Interface> create_interface_instance();
            EC_READING_API_EXPORT void initialise_api();

        } // namespace reading
    }     // namespace streaming
} // namespace ec

#endif // GHOST_TREE_PUBLIC_INCLUDE_EC_GHOST_TREE_API_STREAMING_READER_HPP_
