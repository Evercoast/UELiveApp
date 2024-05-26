// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "MeshBatch.h"
#include "PrimitiveSceneProxy.h"

#include "VoxelVertexFactory.h"
#include "VoxelBuffers.h"

#include "EvercoastMVFVoxelRendererComp.h"

#include <mutex>
#include <memory>

class FVoxelSceneProxy : public FPrimitiveSceneProxy
{
public:
	FVoxelSceneProxy(UEvercoastMVFVoxelRendererComp* Component, std::shared_ptr<EvercoastLocalVoxelFrame> VoxelFrame);
	virtual ~FVoxelSceneProxy();

	SIZE_T GetTypeHash() const override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const;

	virtual uint32 GetMemoryFootprint(void) const override
	{
		return(sizeof(*this) + GetAllocatedSize());
	}

	uint32 GetAllocatedSize(void) const
	{
		return FPrimitiveSceneProxy::GetAllocatedSize();
	}

	/**
	 *	Called when the rendering thread adds the proxy to the scene.
	 *	This function allows for generating renderer-side resources.
	 *	Called in the rendering thread.
	 */
	virtual void CreateRenderThreadResources() override;

	void SetVoxelData_RenderThread(FRHICommandListBase& RHICmdList, std::shared_ptr<EvercoastLocalVoxelFrame> data);

	static FBoxSphereBounds GetDefaultVoxelDataBounds();

	void LockVoxelData();
	void UnlockVoxelData();
private:
	// ~Voxel Data~
	std::shared_ptr<EvercoastLocalVoxelFrame> VoxelFrame;
	mutable std::recursive_mutex	VoxelFrameLock; // need to lock in some const interfaces

	/** The material from the component to render with */
	UMaterialInterface* Material;
	/** The index buffer to use when drawing */
	FVoxelIndexBuffer VoxelIndexBuffer;
	/** The vertex buffer of colors for each point */
	FVoxelColorVertexBuffer VoxelColorVertexBuffer;
	/** The vertex buffer of locations for each point */
	FVoxelPositionVertexBuffer VoxelPositionVertexBuffer;

	FMaterialRelevance MaterialRelevance;
	FVoxelVertexFactory VoxelVertexFactory;
};
