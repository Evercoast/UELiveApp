#include "Gaussian/EvercoastGaussianSplatShadowingSceneProxy.h"
#include "Gaussian/EvercoastGaussianSplatPassthroughResult.h"
#include "Gaussian/EvercoastGaussianSplatShadowCasterComp.h"

FEvercoastGaussianSplatShadowingSceneProxy::FEvercoastGaussianSplatShadowingSceneProxy(const UEvercoastGaussianSplatShadowCasterComp* component, UMaterialInstanceDynamic* material) :
	FEvercoastGaussianSplatSceneProxy(component, material),
	MaterialRelevance(component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
{
}

FEvercoastGaussianSplatShadowingSceneProxy::~FEvercoastGaussianSplatShadowingSceneProxy()
{
}

SIZE_T FEvercoastGaussianSplatShadowingSceneProxy::GetTypeHash() const
{
	// Seems like a best-practice thing for SceneProxy subclasses
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

FPrimitiveViewRelevance FEvercoastGaussianSplatShadowingSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	// For shadow-caster
	FPrimitiveViewRelevance Result;
	Result.bOpaque = true;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bRenderInDepthPass = true;
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bDrawRelevance = IsShown(View);
	Result.bStaticRelevance = false;
	Result.bDynamicRelevance = true;
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
	Result.bVelocityRelevance = false;

	MaterialRelevance.SetPrimitiveViewRelevance(Result); // CRITICAL: translucency relevance from Material settings

	return Result;
}


const FViewMatrices& FEvercoastGaussianSplatShadowingSceneProxy::ExtractRelevantViewMatrices(const FSceneView* pView) const
{
	return pView->ShadowViewMatrices;
}
