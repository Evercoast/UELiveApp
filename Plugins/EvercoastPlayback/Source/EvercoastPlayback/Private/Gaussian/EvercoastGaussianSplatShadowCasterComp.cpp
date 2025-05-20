#include "Gaussian/EvercoastGaussianSplatShadowCasterComp.h"
#include "Gaussian/EvercoastGaussianSplatComputeUploader.h"
#include "Gaussian/EvercoastGaussianSplatDecoder.h"
#include "Gaussian/GaussianSplatComputeShader.h"
#include "Gaussian/EvercoastGaussianSplatShadowingSceneProxy.h"
#include "RHICommandList.h"
#include "RHIUtilities.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "RenderGraphBuilder.h"
#include "Engine/Engine.h"



UEvercoastGaussianSplatShadowCasterComp::UEvercoastGaussianSplatShadowCasterComp(const FObjectInitializer& ObjectInitializer) :
    Super(ObjectInitializer)
{
    CastShadow = true;
    bRenderInMainPass = false;
    bRenderInDepthPass = false;
}



FPrimitiveSceneProxy* UEvercoastGaussianSplatShadowCasterComp::CreateSceneProxy()
{
    if (!SceneProxy)
    {
        MarkDirty();

        GetDataUploader()->MarkDataDirty();

        RecreatedDynamicMaterialFromStaticMaterial();

        FEvercoastGaussianSplatShadowingSceneProxy* newSceneProxy = new FEvercoastGaussianSplatShadowingSceneProxy(this, GetCreatedDynamicMaterial());

        ENQUEUE_RENDER_COMMAND(FEvercoastGaussianDataUpdate)(
            [sceneProxy = newSceneProxy, encodedSplatData = GetRetainedEncodedSplatData()](FRHICommandListImmediate& RHICmdList)
            {
                sceneProxy->SetEncodedGaussianSplat_RenderThread(RHICmdList, encodedSplatData);
            });
        return newSceneProxy;
    }

    return SceneProxy;
}


FBoxSphereBounds UEvercoastGaussianSplatShadowCasterComp::CalcBounds(const FTransform& LocalToWorld) const
{
    FBoxSphereBounds bounds = CalcLocalBounds();

    return bounds.TransformBy(LocalToWorld);
}

FBoxSphereBounds UEvercoastGaussianSplatShadowCasterComp::CalcLocalBounds() const
{
    if (SceneProxy)
    {
        FEvercoastGaussianSplatShadowingSceneProxy* gaussianShadowingSceneProxy = static_cast<FEvercoastGaussianSplatShadowingSceneProxy*>(SceneProxy);
        gaussianShadowingSceneProxy->GetLocalBounds();
    }

    return FEvercoastGaussianSplatShadowingSceneProxy::GetDefaultLocalBounds();
}