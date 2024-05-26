// Copyright (C) 2021 Evercoast Inc. All Rights Reserved.

#ifndef GHOSTTREE_API_API_CODECS_DECODING_PUBLIC_INCLUDE_EC_DECODING_API_TYPES_H_
#define GHOSTTREE_API_API_CODECS_DECODING_PUBLIC_INCLUDE_EC_DECODING_API_TYPES_H_

struct Definition
{
	//LOD0 is the most detailed level in the decoded data. Each LOD will have 3x-4x less data than the last.
	uint8_t required_lod;

	//After decode convert the internal colour data into a single array of RGB colour space (useful for rendering on GPU)
	bool rgb_colourspace_conversion;

	//After decode convert the internal location data into a single array of position data.  (useful for rendering on GPU)
	bool cartesian_space_conversion;

	// XYZW 16 bit uint coordinates
	// RGBA 8 bit uint colour
	bool gfx_api_compatibility_mode;

	// Optimized fixed output decode
	bool fast_decode;

	// 16 bit half float conversion of coordinates
	bool half_float_coordinates;

	// duplicate each output value 4 times
	bool quadruple_output; 

	//If decoding from a mesh, we need to specify the desired distance between points at LOD0
	double desired_lod0_voxel_edge_length_mm;

    bool point_cloud_from_obj;

};

#endif //GHOSTTREE_API_API_CODECS_DECODING_PUBLIC_INCLUDE_EC_DECODING_API_TYPES_H_
