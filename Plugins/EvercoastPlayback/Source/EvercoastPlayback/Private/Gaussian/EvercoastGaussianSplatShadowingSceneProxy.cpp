#include "Gaussian/EvercoastGaussianSplatShadowingSceneProxy.h"
#include "Gaussian/EvercoastGaussianSplatShadowCasterComp.h"

FEvercoastGaussianSplatShadowingSceneProxy::FEvercoastGaussianSplatShadowingSceneProxy(const UEvercoastGaussianSplatShadowCasterComp* component, UMaterialInterface* material,
	bool onlyReconOnTick, float shadowDecimate, float shadowBlobScale) :
	FEvercoastGaussianSplatSceneProxy(component, material, EGaussianSplatRendererType::QUAD_RENDERER, shadowDecimate, 1.0f, 0.0f, false, false, false, false, false, EGaussianSplatHookStage::POST_OPAQUE, 0),
	MaterialRelevance(component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
{
	bPerformLateComputeShaderSplatRecon = onlyReconOnTick;
	SetSplatDecimation(shadowDecimate);
	SetShadowBlobScale(shadowBlobScale);
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

bool FEvercoastGaussianSplatShadowingSceneProxy::ShouldSubmitDynamicMesh(const FSceneView* pView) const
{
	const bool bIsRenderingShadow = pView->ShadowViewMatrices.GetViewMatrix() != pView->ViewMatrices.GetViewMatrix();
	return bIsRenderingShadow;
}