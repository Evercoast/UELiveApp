#pragma once

#include <mutex>
#include "CoreMinimal.h"
#include "EvercoastGaussianSplatVertexFactory.h"
#include "PrimitiveSceneProxy.h"
#include "DynamicMeshBuilder.h"

#if RHI_RAYTRACING
#include "RayTracingDefinitions.h"
#include "RayTracingInstance.h"
#endif

class UEvercoastGaussianSplatComputeComponent;
class EvercoastGaussianSplatPassthroughResult;
class UMaterialInstanceDynamic;

class FEvercoastGaussianSplatSceneProxy : public FPrimitiveSceneProxy
{
public:
	FEvercoastGaussianSplatSceneProxy(const UEvercoastGaussianSplatComputeComponent* component, UMaterialInstanceDynamic* material);
	virtual ~FEvercoastGaussianSplatSceneProxy();

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
	void SetEncodedGaussianSplat_RenderThread(FRHICommandListBase& RHICmdList, std::shared_ptr<const EvercoastGaussianSplatPassthroughResult> data);

	void ResetMaterial(UMaterialInstanceDynamic* material);

	static FBoxSphereBounds GetDefaultLocalBounds();
	FBoxSphereBounds GetLocalBounds() const;

	void LockGaussianData();
	void UnlockGaussianData();

	void SaveEssentialReconData(const FMatrix& ObjectToWorld, const FMatrix& ViewProj, const FMatrix& InView, const FMatrix& InProj, const FVector4& InScreenParam, const FMatrix& InClipToWorld, bool isShadowPass) const; 
	void PerformLateComputeShaderSplatRecon();

	//bool bPerformLateComputeShaderSplatRecon;
protected:
	const FViewMatrices& ExtractRelevantViewMatrices(const FSceneView* pView) const;
private:

	void InitialiseQuadMesh();
	//void InitialiseCubeMesh(); // DEBUG

	// remove constantness requirement in GetDynamicMeshElements() const
	mutable FEvercoastGaussianSplatVertexFactory m_vertexFactory;
	// Splats data
	std::shared_ptr<const EvercoastGaussianSplatPassthroughResult> m_encodedGaussian;
	mutable std::recursive_mutex	m_gaussianFrameLock;

	FStaticMeshVertexBuffers m_quadVertexBuffers;
	FDynamicMeshIndexBuffer32 m_quadIndexBuffer;

	UMaterialInstanceDynamic* m_material;

	/** The view relevance for the gaussian material. Critical for GetViewRelevance() */
	FMaterialRelevance MaterialRelevance;

	// Cached data for recon
	mutable FMatrix SavedObjectToWorld;
	mutable FMatrix SavedViewProj;
	mutable FMatrix SavedView;
	mutable FMatrix SavedProj;
	mutable FVector4 SavedScreenParam;
	mutable FMatrix SavedClipToWorld;
	mutable bool SavedIsShadowPass;

#if RHI_RAYTRACING
	FRayTracingGeometry RayTracingGeometry;
#endif
};