#include "Gaussian/EvercoastGaussianSplatShadowCasterComp.h"
#include "Gaussian/EvercoastGaussianSplatCSUploader.h"
#include "Gaussian/EvercoastGaussianSplatDecoder.h"
#include "Gaussian/GaussianSplatPreprocessComputeShader.h"
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
    rendererType = EGaussianSplatRendererType::QUAD_RENDERER;
    bEnableTileRendererDepthWrite = false;
}


void UEvercoastGaussianSplatShadowCasterComp::SetShadowDecimate(float InSplatDecimate)
{
    ShadowDecimate = InSplatDecimate;

    if (SceneProxy)
    {
        ((FEvercoastGaussianSplatShadowingSceneProxy*)SceneProxy)->SetSplatDecimation(ShadowDecimate);
    }
}

void UEvercoastGaussianSplatShadowCasterComp::SetRendererType(EGaussianSplatRendererType newType)
{
    // always use quad renderer for shadow 
    rendererType = EGaussianSplatRendererType::QUAD_RENDERER;
}

void UEvercoastGaussianSplatShadowCasterComp::SetEnableTileRendererDepthWrite(bool enableDepthWrite)
{
    // it shouldn't matter
    bEnableTileRendererDepthWrite = false;
}

FPrimitiveSceneProxy* UEvercoastGaussianSplatShadowCasterComp::CreateSceneProxy()
{
    if (!SceneProxy)
    {
        MarkDirty();

        GetDataUploader()->MarkDataDirty();

        m_materialInstance = GetNewMaterialInstanceDynamic();

        FEvercoastGaussianSplatShadowingSceneProxy* newSceneProxy = new FEvercoastGaussianSplatShadowingSceneProxy(this, m_materialInstance,
            bReconstructOnTickOnly, ShadowDecimate, SplatExtraScale);

        ENQUEUE_RENDER_COMMAND(FEvercoastGaussianDataUpdate)(
            [sceneProxy = newSceneProxy, encodedSplatData = GetRetainedEncodedSplatData()](FRHICommandListImmediate& RHICmdList)
            {
                sceneProxy->SetEncodedGaussianSplat_RenderThread(RHICmdList, encodedSplatData);
            });
        return newSceneProxy;
    }

    return SceneProxy;
}
