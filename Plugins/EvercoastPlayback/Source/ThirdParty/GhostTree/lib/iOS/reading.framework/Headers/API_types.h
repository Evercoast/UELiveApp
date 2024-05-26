// Copyright (C) 2021 Evercoast Inc. All Rights Reserved.

#ifndef GHOSTTREE_API_API_STREAMING_READER_PUBLIC_INCLUDE_EC_READING_API_TYPES_H_
#define GHOSTTREE_API_API_STREAMING_READER_PUBLIC_INCLUDE_EC_READING_API_TYPES_H_

#include <cstdint>

struct Position
{
    float x;
    float y;
    float z;
};

struct Rotation
{
    float x;
    float y;
    float z;
    float w;
};

struct AABB
{
    Position min;
    Position max;
};

struct RepresentationInfo
{
    uint32_t representation_id;
    uint32_t bit_rate;
    uint32_t sample_rate;
    uint32_t level_of_detail_count;
    const uint32_t* level_of_details;
    const char* external_postfix;
};

struct ChannelInfo
{
    uint32_t channel_id;
    char description[32];
    Position origin;
    Rotation rotation;
    AABB aabb;
    double start_time;
    double duration;
    uint32_t representation_count;
    const RepresentationInfo* representations;
};

struct PlaybackInfo
{
    double duration;
    double segmentDuration; // only set if stream was/is live
    bool isLive;
};

struct ChannelDataBlock
{
    uint32_t cache_id;
    uint32_t block_id;
    uint32_t channel_id;
    uint32_t representation_id;
    uint32_t bit_rate;
    double timestamp;
    double duration;
    uint32_t offset;
    uint32_t size;
};

struct ReadRequest
{
    uint32_t request_id;
    uint32_t cache_id;
    uint8_t* buffer;
    uint64_t offset;
    uint32_t size;
};

struct Config
{
    double initial_request_duration;
    double max_request_duration;
    double initial_cache_duration;
    double max_cache_ahead_duration;
    uint32_t max_cache_memory_size;
    uint32_t format_guess;
    bool debug_logging;
    bool request_meta_data;
    bool progressive_download;
};

#endif // GHOSTTREE_API_API_STREAMING_READER_PUBLIC_INCLUDE_EC_READING_API_TYPES_H_
