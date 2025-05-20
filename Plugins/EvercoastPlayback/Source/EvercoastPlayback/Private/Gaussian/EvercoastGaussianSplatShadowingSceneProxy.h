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
class EvercoastGaussianSplatPassthroughResult;
class UMaterialInstanceDynamic;

class FEvercoastGaussianSplatShadowingSceneProxy : public FEvercoastGaussianSplatSceneProxy
{
public:
	FEvercoastGaussianSplatShadowingSceneProxy(const UEvercoastGaussianSplatShadowCasterComp* component, UMaterialInstanceDynamic* material);
	virtual ~FEvercoastGaussianSplatShadowingSceneProxy();

	/** Return a type (or subtype) specific hash for sorting purposes */
	virtual SIZE_T GetTypeHash() const;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

protected:
	const FViewMatrices& ExtractRelevantViewMatrices(const FSceneView* pView) const;

private:
	/** The view relevance for the gaussian material. Critical for GetViewRelevance() */
	FMaterialRelevance MaterialRelevance;


#if RHI_RAYTRACING
	FRayTracingGeometry RayTracingGeometry;
#endif
};