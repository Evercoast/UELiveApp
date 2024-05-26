/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-13 01:51:05
* @Last Modified by:   Ye Feng
* @Last Modified time: 2021-11-17 05:45:36
*/
#pragma once
#include <cstdint>
#include <memory>
#include "CoreMinimal.h"
#include "UnrealEngineCompatibility.h"
#include "ec_decoder_compatibility.h"


struct EvercoastLocalVoxelFrame
{
	EvercoastLocalVoxelFrame(const GTHandle voxelFrame, bool makeCopy);
	virtual ~EvercoastLocalVoxelFrame();

	uint32_t m_voxelCount;
	FVector3f m_boundsMin;
	float m_boundsDim;
	uint32_t m_bitsPerVoxel;

	uint16_t* m_positionData;
	uint8_t* m_colourData;
	uint32_t m_voxelDataSize;

	bool m_madeCopy;

	FBoxSphereBounds CalcBounds() const;
};
