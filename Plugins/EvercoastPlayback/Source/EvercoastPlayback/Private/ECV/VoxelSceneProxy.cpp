// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelSceneProxy.h"
#include "EvercoastDecoder.h"
#include "EvercoastLocalVoxelFrame.h"

#include "PrimitiveViewRelevance.h"
#include "RHI.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "Containers/ResourceArray.h"
#include "EngineGlobals.h"
#include "VertexFactory.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "LocalVertexFactory.h"
#include "Engine/Engine.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2
#include "Materials/MaterialRenderProxy.h"
#endif

//DECLARE_CYCLE_STAT(TEXT("Update Point Cloud GT"), STAT_Voxel_UpdatePointCloud, STATGROUP_Voxel);
//DECLARE_CYCLE_STAT(TEXT("Get Mesh Elements"), STAT_PointCloud_GetMeshElements, STATGROUP_PointCloud);
//DECLARE_CYCLE_STAT(TEXT("Create RT Resources"), STAT_PointCloud_CreateRenderThreadResources, STATGROUP_PointCloud);

constexpr auto MAX_VOXELS = 1000000u;

FVoxelSceneProxy::FVoxelSceneProxy(UEvercoastMVFVoxelRendererComp* Component, std::shared_ptr<EvercoastLocalVoxelFrame> InVoxelFrame)
	: FPrimitiveSceneProxy(Component)
	, VoxelFrame(InVoxelFrame)
	, Material(Component->VoxelMaterial)
	, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	, VoxelVertexFactory(GetScene().GetFeatureLevel())
{
}

FVoxelSceneProxy::~FVoxelSceneProxy()
{
	VoxelVertexFactory.ReleaseResource();
	VoxelIndexBuffer.ReleaseResource();
	VoxelColorVertexBuffer.ReleaseResource();
	VoxelPositionVertexBuffer.ReleaseResource();
}

void FVoxelSceneProxy::CreateRenderThreadResources()
{
	//SCOPE_CYCLE_COUNTER(STAT_PointCloud_CreateRenderThreadResources);
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
	FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();
	VoxelVertexFactory.InitResource(RHICmdList);
#else
	FRHICommandListBase& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	VoxelVertexFactory.InitResource();
#endif

	VoxelIndexBuffer.InitRHIWithSize(RHICmdList, MAX_VOXELS);
	VoxelPositionVertexBuffer.InitRHIWithSize(RHICmdList, MAX_VOXELS);
	VoxelColorVertexBuffer.InitRHIWithSize(RHICmdList, MAX_VOXELS);
	
	// Setup the vertex factory shader parameters
	FVoxelVertexFactoryParameters UniformParameters;
	UniformParameters.VertexFetch_VoxelPositionBuffer = VoxelPositionVertexBuffer.GetBufferSRV();
	UniformParameters.VertexFetch_VoxelColorBuffer = VoxelColorVertexBuffer.GetBufferSRV();
	VoxelVertexFactory.SetParameters(UniformParameters);

	if (VoxelFrame)
	{
		SetVoxelData_RenderThread(RHICmdList, VoxelFrame);
	}
}

SIZE_T FVoxelSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

void FVoxelSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	//SCOPE_CYCLE_COUNTER(STAT_PointCloud_GetMeshElements);

	std::lock_guard<std::recursive_mutex> guard(VoxelFrameLock);
	if (!VoxelFrame || VoxelFrame->m_voxelCount == 0 || !Material)
		return;

	const bool bIsWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
	FMaterialRenderProxy* MaterialProxy = Material->GetRenderProxy();
	if (bIsWireframe)
	{
		FMaterialRenderProxy* WireframeMaterialInstance = new FColoredMaterialRenderProxy(
				GEngine->WireframeMaterial->GetRenderProxy(),
				FLinearColor(0, 0.5f, 1.f)
		);
		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
		MaterialProxy = WireframeMaterialInstance;
	}
	// Nothing to render with
	if (MaterialProxy == nullptr)
	{
		return;
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			if (bIsWireframe)
			{
				// Draw bounds around the points
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
			}

			// Create a mesh batch for this chunk of point cloud
			FMeshBatch& MeshBatch = Collector.AllocateMesh();
			MeshBatch.CastShadow = true;
			MeshBatch.bUseAsOccluder = false;
			MeshBatch.VertexFactory = &VoxelVertexFactory;
			MeshBatch.MaterialRenderProxy = MaterialProxy;
			MeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
			MeshBatch.DepthPriorityGroup = SDPG_World;

			// Set up index buffer
			MeshBatch.Type = VoxelIndexBuffer.IsTriList() ? PT_TriangleList : PT_QuadList;
			FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
			BatchElement.FirstIndex = 0;
			BatchElement.NumPrimitives = VoxelIndexBuffer.GetNumPrimitives();
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = VoxelIndexBuffer.GetMaxIndex();
			BatchElement.IndexBuffer = &VoxelIndexBuffer;
			BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();

			MeshBatch.bCanApplyViewModeOverrides = false;
			Collector.AddMesh(ViewIndex, MeshBatch);
		}
	}
}

void FVoxelSceneProxy::SetVoxelData_RenderThread(FRHICommandListBase& RHICmdList, std::shared_ptr<EvercoastLocalVoxelFrame> data)
{
	check(IsInRenderingThread());

	std::lock_guard<std::recursive_mutex> guard(VoxelFrameLock);

	VoxelFrame = data;

	if (VoxelFrame->m_voxelCount > 0) {
		const auto voxelCount = std::min(VoxelFrame->m_voxelCount, MAX_VOXELS);
		VoxelPositionVertexBuffer.Update(RHICmdList, VoxelFrame->m_positionData, voxelCount);
		VoxelColorVertexBuffer.Update(RHICmdList, VoxelFrame->m_colourData, voxelCount);
		VoxelIndexBuffer.SetNumPoints(voxelCount);
		const auto Rescale = (1.0f / ((1 << VoxelFrame->m_bitsPerVoxel) - 1)) * VoxelFrame->m_boundsDim;
		VoxelVertexFactory.SetParameters(VoxelFrame->m_boundsMin * 100.0f, Rescale * 100.0f);
	}
}

FPrimitiveViewRelevance FVoxelSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	/*FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = true;
	Result.bDynamicRelevance = true;
	Result.bRenderInMainPass = true;
	Result.bUsesLightingChannels = false;
	Result.bRenderCustomDepth = false;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	return Result;*/

	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
	return Result;
}

FBoxSphereBounds FVoxelSceneProxy::GetDefaultVoxelDataBounds()
{
	return FBoxSphereBounds(FBox(FVector(-200, -50, -200), FVector(200, 300, 200)));;
}