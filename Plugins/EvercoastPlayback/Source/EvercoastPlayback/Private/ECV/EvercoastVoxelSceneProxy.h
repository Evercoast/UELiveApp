#pragma once

#include <mutex>
#include "CoreMinimal.h"
#include "EvercoastVoxelRendererComp.h"
#include "LocalVertexFactory.h"
#include "EvercoastInstancedCubeVertexFactory.h"
#include "Filter/FilteringTarget.h"
#include "PrimitiveSceneProxy.h"
#include "DynamicMeshBuilder.h"

#if RHI_RAYTRACING
#include "RayTracingDefinitions.h"
#include "RayTracingInstance.h"
#endif


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEvercoastVoxelSceneProxy
///////////////////////////////////////////////////////////////////////////////////////////////////////////
struct EvercoastLocalVoxelFrame;
class UMaterialInstanceDynamic;
class FPositionOnlyVertexDeclaration;
class FPositionOnlyVertexBuffer;
class FEvercoastVoxelSceneProxy final : public FPrimitiveSceneProxy
{
public:

	FEvercoastVoxelSceneProxy(UEvercoastVoxelRendererComp* component, UMaterialInstanceDynamic* material, bool generateNormal, bool useIcosahedron,
		float sizeFactor, int32 smoothIteration, UTextureRenderTarget2D* captureRenderTarget_L, UTextureRenderTarget2D* captureRenderTarget_R, 
		UTextureRenderTarget2D* outputNormalTarget_L, UTextureRenderTarget2D* outputNormalTarget_R);
	virtual ~FEvercoastVoxelSceneProxy();

	/** Return a type (or subtype) specific hash for sorting purposes */
	virtual SIZE_T GetTypeHash() const;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

#if RHI_RAYTRACING
	virtual bool IsRayTracingRelevant() const override { return true; }

#if ENGINE_MAJOR_VERSION == 5
	virtual bool HasRayTracingRepresentation() const override { return true; }
#endif

	virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances) override;
#endif

	virtual uint32 GetMemoryFootprint(void) const override;
	uint32 GetAllocatedSize(void) const;
	void SetVoxelData_RenderThread(FRHICommandListBase& RHICmdList, std::shared_ptr<EvercoastLocalVoxelFrame> data);

	FBoxSphereBounds GetVoxelDataBounds() const;

	static FBoxSphereBounds GetDefaultVoxelDataBounds();

	void LockVoxelData();
	void UnlockVoxelData();

	void ResetMaterial(UMaterialInstanceDynamic* material);

private:

	void InitialiseCubeMesh();
	void InitialiseIcosahedronMesh();
	

	static void FilterWorldNormal_RenderThread(const FFlipFilterRenderTarget* FilterRenderTarget, const FTexture& SrcTexture, const FTexture& DstTexture, int FilterIteration, FRHICommandListImmediate& RHICmdList);
	static void RenderWorldNormal_RenderThread(const FTexture& DestTexture, const FGenericDepthTarget* NormalRender_DepthTarget,
		const FEvercoastInstancedCubeVertexFactory& vertexFactory, const FPositionVertexBuffer& PositionVertexBuffer, const FDynamicMeshIndexBuffer32& IndexBuffer,
		FMatrix ObjectToCamera, FMatrix CameraToWorld, FMatrix ObjectToProjection, std::shared_ptr<EvercoastLocalVoxelFrame> voxelFrame, FRHICommandListImmediate& RHICmdList);

	FStaticMeshVertexBuffers VertexBuffers_Cube;
	FDynamicMeshIndexBuffer32 IndexBuffer_Cube;

	FStaticMeshVertexBuffers VertexBuffers_Icosahedron;
	FDynamicMeshIndexBuffer32 IndexBuffer_Icosahedron;

	FEvercoastInstancedCubeVertexFactory VertexFactory_CubeMesh;
	FEvercoastInstancedCubeVertexFactory VertexFactory_IcosahedronMesh;

	UMaterialInstanceDynamic* Material;

	// ~Voxel Data~
	std::shared_ptr<EvercoastLocalVoxelFrame> m_voxelFrame;
	mutable std::recursive_mutex	m_voxelFrameLock; // need to lock in some const interfaces

	// Normal generation
	bool bNormalRender;
	bool bUseIcosahedronForNormalRender;
	float MeshSizeFactor;
	TSharedPtr<FGenericDepthTarget> NormalRender_DepthTarget;
	TSharedPtr<FFlipFilterRenderTarget> NormalRender_FilterTarget;
	int32 NormalRender_SmoothIteration;
	TWeakObjectPtr<UTextureRenderTarget2D> CaptureRenderTarget[2];
	TWeakObjectPtr<UTextureRenderTarget2D> FilteredNormalRenderTarget[2];
	USceneComponent* BaseComponent;

#if RHI_RAYTRACING
	FRayTracingGeometry RayTracingGeometry;
#endif

};
