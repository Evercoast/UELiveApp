#pragma once

#include <mutex>
#include "CoreMinimal.h"
#include "EvercoastGaussianSplatVertexFactory.h"
#include "PrimitiveSceneProxy.h"
#include "DynamicMeshBuilder.h"
#include "Gaussian/EvercoastGaussianSplatSceneProxy.h"

#if RHI_RAYTRACING
#include "RayTracingDefinitions.h"
#include "RayTracingInstance.h"
#endif

class UEvercoastGaussianSplatShadowCasterComp;
class FEvercoastGaussianSplatShadowingSceneProxy : public FEvercoastGaussianSplatSceneProxy
{
public:
	FEvercoastGaussianSplatShadowingSceneProxy(const UEvercoastGaussianSplatShadowCasterComp* component, UMaterialInterface* material, bool onlyReconOnTick, float shadowDecimate, float shadowBlobScale);
	virtual ~FEvercoastGaussianSplatShadowingSceneProxy();

	/** Return a type (or subtype) specific hash for sorting purposes */
	virtual SIZE_T GetTypeHash() const;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

protected:
	// Return shadow view matrix
	virtual const FViewMatrices& ExtractRelevantViewMatrices(const FSceneView* pView) const override;
	virtual bool ShouldSubmitDynamicMesh(const FSceneView* pView) const override;

private:
	/** The view relevance for the gaussian material. Critical for GetViewRelevance() */
	FMaterialRelevance MaterialRelevance;


#if RHI_RAYTRACING
	FRayTracingGeometry RayTracingGeometry;
#endif
};