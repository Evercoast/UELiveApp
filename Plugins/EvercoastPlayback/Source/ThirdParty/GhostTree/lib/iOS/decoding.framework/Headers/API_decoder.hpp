// Copyright (C) 2020 Evercoast Inc. All Rights Reserved.

#ifndef GHOST_TREE_PUBLIC_INCLUDE_EC_GHOST_TREE_API_DECODER_HPP_
#define GHOST_TREE_PUBLIC_INCLUDE_EC_GHOST_TREE_API_DECODER_HPP_

#include <ec/decoding/API_macros.hpp>

#include <vector>

namespace ec
{
namespace codecs
{
namespace decoding
{

#include "ec/decoding/API_types.h"

class PointCloudFrame
{
public:
	PointCloudFrame() = default;
	virtual ~PointCloudFrame() = default;

	[[nodiscard]] virtual int64_t get_frame_number() const = 0;
	[[nodiscard]] virtual uint32_t get_data_item_count() const = 0;
	[[nodiscard]] virtual const float* get_positions() const = 0;
	[[nodiscard]] virtual const uint8_t* get_colours() const = 0;
};

class VoxelCloudFrame
{
public:
	VoxelCloudFrame() = default;
	virtual ~VoxelCloudFrame() = default;

	[[nodiscard]] virtual int64_t get_frame_number() const = 0;
	[[nodiscard]] virtual uint32_t get_data_item_count() const = 0;

	[[nodiscard]] virtual float get_bounds_min_X() const = 0;
	[[nodiscard]] virtual float get_bounds_min_Z() const = 0;
	[[nodiscard]] virtual float get_bounds_min_Y() const = 0;

	[[nodiscard]] virtual float get_bounds_dim() const = 0;
	[[nodiscard]] virtual uint32_t get_bits_per_voxel() const = 0;

	[[nodiscard]] virtual const uint16_t* get_coordinates() const = 0;
	[[nodiscard]] virtual const uint8_t* get_colours() const = 0;

	[[nodiscard]] virtual uint32_t get_coordinate_elements() const = 0;
	[[nodiscard]] virtual uint32_t get_colour_elements() const = 0;

	[[nodiscard]] virtual bool copy_coordinates(uint16_t* ptr) const = 0;
	[[nodiscard]] virtual bool copy_colours(uint8_t* ptr) const = 0;
};

class HierarchicalFrame
{
public:
	HierarchicalFrame() = default;
	virtual ~HierarchicalFrame() = default;

	[[nodiscard]] virtual int64_t get_frame_number() const = 0;

	[[nodiscard]] virtual float get_bounds_min_X() const = 0;
	[[nodiscard]] virtual float get_bounds_min_Y() const = 0;
	[[nodiscard]] virtual float get_bounds_min_Z() const = 0;

	[[nodiscard]] virtual float get_bounds_dim() const = 0;
	[[nodiscard]] virtual uint32_t get_level_count() const = 0;
	[[nodiscard]] virtual float get_leaf_voxel_edge_mm() const = 0;

	virtual HANDLE get_api_data_handle() = 0;
	//-------------------------------------------------------------------
	// Todo:  Add many accessor functions so that the internal data is  available for use.
	//
	// For now the only viable use case for HierarchicalFrame is within the encoder. Decode
	// to HierarchicalFrame then pass  HierarchicalFrame back into an Encoder for an ultra fast path.
	//-------------------------------------------------------------------
};

class MeshFrameHeaderInfo
{
public:
    MeshFrameHeaderInfo() = default;
    virtual ~MeshFrameHeaderInfo() = default;

    [[nodiscard]] virtual int64_t get_frame_number() const = 0;
    [[nodiscard]] virtual uint32_t get_vertex_count() const = 0;
    [[nodiscard]] virtual uint32_t get_triangle_count() const = 0;
    [[nodiscard]] virtual uint32_t get_index_count() const = 0;
    [[nodiscard]] virtual uint32_t get_frame_type() const = 0;
    [[nodiscard]] virtual uint32_t get_keyframe_number() const = 0;

    [[nodiscard]] virtual float get_bounds_min_X() const = 0;
    [[nodiscard]] virtual float get_bounds_min_Z() const = 0;
    [[nodiscard]] virtual float get_bounds_min_Y() const = 0;

    [[nodiscard]] virtual float get_bounds_dim_X() const = 0;
    [[nodiscard]] virtual float get_bounds_dim_Y() const = 0;
    [[nodiscard]] virtual float get_bounds_dim_Z() const = 0;
#if !defined(EMSCRIPTEN) && !defined(__cplusplus_winrt) && !defined(ANDROID) && !defined(__APPLE__)
    [[nodiscard]] virtual uint32_t get_texture_dim_X() const = 0;
    [[nodiscard]] virtual uint32_t get_texture_dim_Y() const = 0;
#endif
};

class MeshFrame
{
public:
    MeshFrame() = default;
    virtual ~MeshFrame() = default;

    [[nodiscard]] virtual const MeshFrameHeaderInfo& get_mesh_frame_header_info() const = 0;
    [[nodiscard]] virtual float* get_vertex_positions() const = 0;
    [[nodiscard]] virtual uint32_t* get_triangle_indices() const = 0;
    [[nodiscard]] virtual float* get_vertex_uvs() const = 0;
    [[nodiscard]] virtual float* get_vertex_normals() const = 0;
#if !defined(EMSCRIPTEN) && !defined(__cplusplus_winrt) && !defined(ANDROID) && !defined(__APPLE__)
    [[nodiscard]] virtual uint8_t* get_texture_data() const = 0;
#endif
};

class VoxelFrameHeaderInfo
{
public:
	virtual ~VoxelFrameHeaderInfo() = default;

	[[nodiscard]] virtual float get_voxel_cube_bounds_min_X() const = 0;
	[[nodiscard]] virtual float get_voxel_cube_bounds_min_Y() const = 0;
	[[nodiscard]] virtual float get_voxel_cube_bounds_min_Z() const = 0;
	[[nodiscard]] virtual float get_voxel_cube_bounds_dim() const = 0;

	[[nodiscard]] virtual float get_tight_bounds_min_X() const = 0;
	[[nodiscard]] virtual float get_tight_bounds_min_Y() const = 0;
	[[nodiscard]] virtual float get_tight_bounds_min_Z() const = 0;

	[[nodiscard]] virtual float get_tight_bounds_dim_X() const = 0;
	[[nodiscard]] virtual float get_tight_bounds_dim_Y() const = 0;
	[[nodiscard]] virtual float get_tight_bounds_dim_Z() const = 0;

	[[nodiscard]] virtual uint32_t get_min_valid_level() const = 0;
	[[nodiscard]] virtual uint32_t get_max_valid_level() const = 0;
	[[nodiscard]] virtual std::vector<uint32_t> get_voxel_counts_in_levels() const = 0;
};

class Interface
{
public:
	virtual ~Interface() = default;

	virtual bool openFrom(const char* path) = 0;
	virtual bool open(const char* stream, size_t stream_byte_count) = 0;
	virtual bool open(uintptr_t stream, size_t stream_byte_count) = 0;

	virtual Definition default_definition() = 0;
	virtual bool decode_from_stream(const Definition& stream_def) = 0;

	virtual uint32_t get_maximum_header_read_size() = 0;
    virtual bool get_header_from_stream(std::shared_ptr<VoxelFrameHeaderInfo>& header_data) = 0;
    virtual bool get_header_from_stream(std::shared_ptr<MeshFrameHeaderInfo>& header_data) = 0;

	virtual bool get_data(std::shared_ptr<VoxelCloudFrame>& decoded_voxel_data) = 0;

	virtual bool get_data(std::shared_ptr<PointCloudFrame>& decoded_point_data) = 0;

	virtual bool get_data(std::shared_ptr<HierarchicalFrame>& decoded_hierarchical_data) = 0;

    virtual bool get_data(std::shared_ptr<MeshFrame>& decoded_mesh_data) = 0;

	virtual void close() = 0;

protected:
	Interface();
};

EC_DECODING_API_EXPORT void initialise_internal_services_registry(void* registry);
EC_DECODING_API_EXPORT std::shared_ptr<Interface> create_interface_instance();
EC_DECODING_API_EXPORT void initialise_api();
EC_DECODING_API_EXPORT void deinitialise_api();

}  // namespace decoder
}  // namespace codecs
}  // namespace ec

#endif  // GHOST_TREE_PUBLIC_INCLUDE_EC_GHOST_TREE_API_HPP_
