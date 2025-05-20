#ifndef GHOST_TREE_PUBLIC_INCLUDE_EC_GHOST_TREE_API_C_DECODER_H_
#define GHOST_TREE_PUBLIC_INCLUDE_EC_GHOST_TREE_API_C_DECODER_H_

#include <stdint.h>

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

    EC_DECODING_API_EXPORT void initialise_decoding_api();
    EC_DECODING_API_EXPORT void deinitialise_decoding_api();

    struct DecodeHeader
    {
        uint32_t frameNumber;
        uint32_t vertexCount;
        uint32_t indexCount;
    };

    EC_DECODING_API_EXPORT Handle create_mesh_decoder(uint8_t* encodedStream, uint32_t encodedStreamSize, DecodeHeader* header, uint32_t headerSize);
    EC_DECODING_API_EXPORT bool release_mesh_decoder(Handle decoder);
    EC_DECODING_API_EXPORT bool decode_mesh(Handle decoder, float* positions, float* uvs, float* normals, uint32_t* indices);

    // Old API

    // Interface
    EC_DECODING_API_EXPORT Handle create_decoder_instance();
    EC_DECODING_API_EXPORT bool release_decoder_instance(Handle decoder);

    EC_DECODING_API_EXPORT bool decoder_open(Handle decoder, uintptr_t stream, uint32_t stream_byte_count);

    EC_DECODING_API_EXPORT Definition decoder_default_decode_definition(Handle decoder);

    EC_DECODING_API_EXPORT bool decoder_decode(Handle decoder, Definition definition);

    EC_DECODING_API_EXPORT uint32_t decoder_get_maximum_header_read_size(Handle decoder);
    EC_DECODING_API_EXPORT Handle decoder_get_frame_header_info(Handle decoder);

    EC_DECODING_API_EXPORT Handle decoder_get_voxel_frame(Handle decoder);

    EC_DECODING_API_EXPORT void decoder_close(Handle decoder);

    // Voxel Frame
    EC_DECODING_API_EXPORT bool release_voxel_frame_instance(Handle voxel_frame);

    struct VoxelFrameDefinition
    {
        int64_t frame_number;
        uint32_t voxel_count;
        float bounds_min_x;
        float bounds_min_y;
        float bounds_min_z;
        float bounds_dim;
        uint32_t bits_per_voxel;
    };

    EC_DECODING_API_EXPORT bool voxel_frame_get_definition(Handle voxel_frame, VoxelFrameDefinition* definition);

    EC_DECODING_API_EXPORT uintptr_t voxel_frame_get_coordinates(Handle voxel_frame);
    EC_DECODING_API_EXPORT uintptr_t voxel_frame_get_colours(Handle voxel_frame);

    EC_DECODING_API_EXPORT bool voxel_frame_copy_coordinates(Handle voxel_frame, uintptr_t ptr);
    EC_DECODING_API_EXPORT bool voxel_frame_copy_colours(Handle voxel_frame, uintptr_t ptr);

    // Frame Header Info
    EC_DECODING_API_EXPORT bool release_frame_header_info_instance(Handle fhi);

    struct FrameHeaderInfo
    {
        float voxel_cube_bounds_min_x;
        float voxel_cube_bounds_min_y;
        float voxel_cube_bounds_min_z;
        float voxel_cube_bounds_dim;

        float tight_bounds_min_x;
        float tight_bounds_min_y;
        float tight_bounds_min_z;
        float tight_bounds_dim_x;
        float tight_bounds_dim_y;
        float tight_bounds_dim_z;

        uint32_t min_valid_level;
        uint32_t max_valid_level;
    };

    EC_DECODING_API_EXPORT bool frame_header_info_get_info(Handle fhi, FrameHeaderInfo* out);
    EC_DECODING_API_EXPORT uint32_t frame_header_info_level_voxel_counts(Handle fhi, uint32_t* out, uint32_t size);

#ifdef __cplusplus
};
#endif

#endif // GHOST_TREE_PUBLIC_INCLUDE_EC_GHOST_TREE_API_C_DECODER_H_
