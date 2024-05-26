/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-13 01:51:05
* @Last Modified by:   Ye Feng
* @Last Modified time: 2021-11-17 05:47:10
*/
#include "EvercoastLocalVoxelFrame.h"
#include "EvercoastDecoder.h"

EvercoastLocalVoxelFrame::EvercoastLocalVoxelFrame(const GTHandle voxelFrame, bool makeCopy) :
	m_voxelCount(0),
	m_boundsMin(FVector3f()),
	m_boundsDim(0),
	m_bitsPerVoxel(0),
	m_positionData(nullptr),
	m_colourData(nullptr),
	m_voxelDataSize(0),
	m_madeCopy(makeCopy)
{
	VoxelFrameDefinition frameDef;
	if (voxel_frame_get_definition(voxelFrame, &frameDef))
	{
		m_voxelCount = frameDef.voxel_count;
		m_boundsMin = FVector3f(frameDef.bounds_min_x, frameDef.bounds_min_y, frameDef.bounds_min_z);
		m_boundsDim = frameDef.bounds_dim;
		m_bitsPerVoxel = frameDef.bits_per_voxel;
		m_voxelDataSize = m_voxelCount * (DECODER_POSITION_ELEMENT_SIZE + DECODER_COLOUR_ELEMENT_SIZE);

		if (makeCopy)
		{
			m_positionData = new uint16_t[m_voxelCount * DECODER_POSITION_ELEMENT_SIZE];
			m_colourData = new uint8_t[m_voxelCount * DECODER_COLOUR_ELEMENT_SIZE];

			FMemory::Memcpy(m_positionData, reinterpret_cast<uint16_t*>(voxel_frame_get_coordinates(voxelFrame)), m_voxelCount * DECODER_POSITION_ELEMENT_SIZE);
			FMemory::Memcpy(m_colourData, reinterpret_cast<uint8_t*>(voxel_frame_get_colours(voxelFrame)), m_voxelCount * DECODER_COLOUR_ELEMENT_SIZE);
		}
		else
		{
			m_positionData = reinterpret_cast<uint16_t*>(voxel_frame_get_coordinates(voxelFrame));
			m_colourData = reinterpret_cast<uint8_t*>(voxel_frame_get_colours(voxelFrame));
		}
	}
	else
	{
		UE_LOG(EvercoastDecoderLog, Warning, TEXT("Decode failed. Cannot extract frame def while uploading. Empty frame will be used."));
	}
}

EvercoastLocalVoxelFrame::~EvercoastLocalVoxelFrame()
{
	if (m_madeCopy)
	{
		delete[] m_positionData;
		delete[] m_colourData;
	}
}


FBoxSphereBounds EvercoastLocalVoxelFrame::CalcBounds() const
{
	const float EVERCOAST_TO_UNREAL = 100.0f;
	FVector3f boundsMin(m_boundsMin.X, m_boundsMin.Z, m_boundsMin.Y); // convert system to XZY
	FVector3f boundsMax(boundsMin.X + m_boundsDim, boundsMin.Y + m_boundsDim, boundsMin.Z + m_boundsDim);

	boundsMin *= EVERCOAST_TO_UNREAL;
	boundsMax *= EVERCOAST_TO_UNREAL;
	return FBoxSphereBounds(FBox(boundsMin, boundsMax));
}
