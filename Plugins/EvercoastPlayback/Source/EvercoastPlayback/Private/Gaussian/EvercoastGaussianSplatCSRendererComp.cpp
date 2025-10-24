#include "Gaussian/EvercoastGaussianSplatCSRendererComp.h"
#include "Gaussian/EvercoastGaussianSplatCSUploader.h"
#include "Gaussian/EvercoastGaussianSplatDecoder.h"
#include "Gaussian/GaussianSplatPreprocessComputeShader.h"
#include "Gaussian/EvercoastGaussianSplatSceneProxy.h"
#include "EvercoastVoxelDecoder.h" // log define
#include "RHICommandList.h"
#include "RHIUtilities.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "RenderGraphBuilder.h"
#include "Engine/Engine.h"
#include "MaterialDomain.h"


UEvercoastGaussianSplatCSRendererComp::UEvercoastGaussianSplatCSRendererComp(const FObjectInitializer& ObjectInitializer) :
    Super(ObjectInitializer),
    m_dirtyMark(true)
{
    CastShadow = false;
    PrimaryComponentTick.bCanEverTick = true;
    bTickInEditor = true; // need this to tick and subsequently call MarkRenderTransformDirty()
    bUseAttachParentBound = false;


    m_dataUploader = std::make_shared<EvercoastGaussianSplatCSUploader>(this);

    m_materialDirty = true;
}

std::shared_ptr<IEvercoastStreamingDataUploader> UEvercoastGaussianSplatCSRendererComp::GetDataUploader() const
{
    return m_dataUploader;
}



void UEvercoastGaussianSplatCSRendererComp::MarkDirty()
{
    m_dirtyMark = true;
}

void UEvercoastGaussianSplatCSRendererComp::SetGaussianSplatMaterial(UMaterialInterface* newMaterial)
{
    if (GaussianSplatMaterial != newMaterial)
    {
        GaussianSplatMaterial = newMaterial;

        m_materialInstance = GetNewMaterialInstanceDynamic();

        MarkRenderStateDirty();

        m_materialDirty = true;
    }
}

void UEvercoastGaussianSplatCSRendererComp::SetReconstructOnTickOnly(bool newValue)
{
    bReconstructOnTickOnly = newValue;

    if (SceneProxy)
    {
        ((FEvercoastGaussianSplatSceneProxy*)SceneProxy)->bPerformLateComputeShaderSplatRecon = bReconstructOnTickOnly;
    }
}

void UEvercoastGaussianSplatCSRendererComp::SetSplatExtraScale(float InSplatExtraScale)
{
    SplatExtraScale = InSplatExtraScale;

    if (SceneProxy)
    {
        ((FEvercoastGaussianSplatSceneProxy*)SceneProxy)->SetSplatExtraScale(SplatExtraScale);
    }
}


void UEvercoastGaussianSplatCSRendererComp::SetCov2DSqrtKernelSize(float InKernelSize)
{
    Cov2DSqrtKernelSize = InKernelSize;

    if (SceneProxy)
    {
        ((FEvercoastGaussianSplatSceneProxy*)SceneProxy)->SetCov2DSqrtKernelSize(Cov2DSqrtKernelSize);
    }
}


void UEvercoastGaussianSplatCSRendererComp::SetShowSphericalHarmonics0(bool show)
{
    bShowDiffuseColour = show;

    if (SceneProxy)
    {
        ((FEvercoastGaussianSplatSceneProxy*)SceneProxy)->SetShowSphericalHarmonics0(show);
    }
}

void UEvercoastGaussianSplatCSRendererComp::SetShowSphericalHarmonics1(bool show)
{
    bShowSphericalHarmonics1Colour = show;

    if (SceneProxy)
    {
        ((FEvercoastGaussianSplatSceneProxy*)SceneProxy)->SetShowSphericalHarmonics1(show);
    }
}

void UEvercoastGaussianSplatCSRendererComp::SetShowSphericalHarmonics2(bool show)
{
    bShowSphericalHarmonics2Colour = show;

    if (SceneProxy)
    {
        ((FEvercoastGaussianSplatSceneProxy*)SceneProxy)->SetShowSphericalHarmonics2(show);
    }
}

void UEvercoastGaussianSplatCSRendererComp::SetShowSphericalHarmonics3(bool show)
{
    bShowSphericalHarmonics3Colour = show;

    if (SceneProxy)
    {
        ((FEvercoastGaussianSplatSceneProxy*)SceneProxy)->SetShowSphericalHarmonics3(show);
    }
}



void UEvercoastGaussianSplatCSRendererComp::SetRendererType(EGaussianSplatRendererType newType)
{
    rendererType = newType;

    if (SceneProxy)
    {
        ((FEvercoastGaussianSplatSceneProxy*)SceneProxy)->SetRendererType(newType);
    }
}

void UEvercoastGaussianSplatCSRendererComp::SetEnableTileRendererDepthWrite(bool enableDepthWrite)
{
    bEnableTileRendererDepthWrite = enableDepthWrite;

    if (SceneProxy)
    {
        ((FEvercoastGaussianSplatSceneProxy*)SceneProxy)->EnableTileRendererDepthWrite(enableDepthWrite);
    }
}

void UEvercoastGaussianSplatCSRendererComp::SetTileRendererHookStage(EGaussianSplatHookStage stage)
{
    TileRendererHookStage = stage;

    if (SceneProxy)
    {
        ((FEvercoastGaussianSplatSceneProxy*)SceneProxy)->SetTileRendererHookStage(stage);
    }
}

UMaterialInstanceDynamic* UEvercoastGaussianSplatCSRendererComp::GetNewMaterialInstanceDynamic()
{
    UMaterialInstanceDynamic* newMaterial = CreateAndSetMaterialInstanceDynamicFromMaterial(0, GaussianSplatMaterial);
    return newMaterial;
}

FPrimitiveSceneProxy* UEvercoastGaussianSplatCSRendererComp::CreateSceneProxy()
{
    if (!SceneProxy)
    {
        MarkDirty();

        m_dataUploader->MarkDataDirty();

        m_materialInstance = GetNewMaterialInstanceDynamic();

        FEvercoastGaussianSplatSceneProxy* newSceneProxy = new FEvercoastGaussianSplatSceneProxy(this, m_materialInstance,
            rendererType,
            0.0f,                   // decimation
            SplatExtraScale,        // quad scale override
            Cov2DSqrtKernelSize,    // small gaussian gets blurred and boosted,
            bShowDiffuseColour,
            bShowSphericalHarmonics1Colour, bShowSphericalHarmonics2Colour, bShowSphericalHarmonics3Colour,
            bEnableTileRendererDepthWrite, TileRendererHookStage
        );

        newSceneProxy->bPerformLateComputeShaderSplatRecon = bReconstructOnTickOnly;

        ENQUEUE_RENDER_COMMAND(FEvercoastGaussianDataUpdate)(
            [sceneProxy = newSceneProxy, encodedSplatData = GetRetainedEncodedSplatData()](FRHICommandListImmediate& RHICmdList)
            {
                sceneProxy->SetEncodedGaussianSplat_RenderThread(RHICmdList, encodedSplatData);
            });
        return newSceneProxy;
    }

    return SceneProxy;
}

int32 UEvercoastGaussianSplatCSRendererComp::GetNumMaterials() const
{
    if (m_materialInstance)
        return 1;
    return 0;
}

UMaterialInterface* UEvercoastGaussianSplatCSRendererComp::GetMaterial(int32 ElementIndex) const
{
    if (GaussianSplatMaterial)
        return GaussianSplatMaterial; // need to return the "vanilla" material to be able to create dynamic material instance

    return UMaterial::GetDefaultMaterial(MD_Surface);
}

void UEvercoastGaussianSplatCSRendererComp::GetUsedMaterials(TArray< UMaterialInterface* >& OutMaterials, bool bGetDebugMaterials) const
{
    if (m_materialInstance)
        OutMaterials.Add(m_materialInstance);

    if (SceneProxy && m_materialDirty)
    {
        ((FEvercoastGaussianSplatSceneProxy*)SceneProxy)->ResetMaterial(m_materialInstance);
        m_materialDirty = false;
    }
}



void UEvercoastGaussianSplatCSRendererComp::SetGaussianSplatData(std::shared_ptr<const EvercoastGaussianSplatCSResult> splatData)
{
    // Retain splat passthrough result
    m_retainedEncodedSplatData = splatData;

    FEvercoastGaussianSplatSceneProxy* sceneProxy = (FEvercoastGaussianSplatSceneProxy*)(this->SceneProxy);

    if (sceneProxy)
    {
        ENQUEUE_RENDER_COMMAND(FEvercoastGaussianSplatUpload)(
            [sceneProxy, encodedSplatData = m_retainedEncodedSplatData](FRHICommandListImmediate& RHICmdList)
            {
                sceneProxy->SetEncodedGaussianSplat_RenderThread(RHICmdList, encodedSplatData);
            });
    }
}



FBoxSphereBounds UEvercoastGaussianSplatCSRendererComp::CalcBounds(const FTransform& LocalToWorld) const
{
    FBoxSphereBounds bounds = CalcLocalBounds();

    return bounds.TransformBy(LocalToWorld);
}

FBoxSphereBounds UEvercoastGaussianSplatCSRendererComp::CalcLocalBounds() const
{
    if (SceneProxy)
    {
        FEvercoastGaussianSplatSceneProxy* gaussianSceneProxy = static_cast<FEvercoastGaussianSplatSceneProxy*>(SceneProxy);
        gaussianSceneProxy->GetLocalBounds();
    }

    return FEvercoastGaussianSplatSceneProxy::GetDefaultLocalBounds();
}

#if WITH_EDITOR
void UEvercoastGaussianSplatCSRendererComp::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    SceneProxy = nullptr;
    
    MarkRenderStateDirty(); // recreate sceneproxy by end of frame

    Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UEvercoastGaussianSplatCSRendererComp::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (m_dataUploader->IsDataDirty())
    {
        m_dataUploader->ForceUpload();
    }

    FEvercoastGaussianSplatSceneProxy* SplatSceneProxy = ((FEvercoastGaussianSplatSceneProxy*)SceneProxy);

    if (SceneProxy && bReconstructOnTickOnly)
    {
        SplatSceneProxy->PerformLateComputeShaderSplatRecon();
    }

    if (m_dirtyMark)
    {
        UpdateBounds();

        MarkRenderTransformDirty();
        m_dirtyMark = false;
    }
}

FMaterialRelevance UEvercoastGaussianSplatCSRendererComp::GetMaterialRelevance(ERHIFeatureLevel::Type InFeatureLevel) const
{
    // Combine the material relevance for all materials.
    FMaterialRelevance Result;
    for (int32 ElementIndex = 0; ElementIndex < GetNumMaterials(); ElementIndex++)
    {
        UMaterialInterface const* MaterialInterface = GetMaterial(ElementIndex);
        if (!MaterialInterface)
        {
            MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
        }
        Result |= MaterialInterface->GetRelevance_Concurrent(InFeatureLevel);
    }

    return Result;

}

std::shared_ptr<const EvercoastGaussianSplatCSResult> UEvercoastGaussianSplatCSRendererComp::GetRetainedEncodedSplatData() const
{
    return m_retainedEncodedSplatData;
}

void UEvercoastGaussianSplatCSRendererComp::OnRegister()
{
    Super::OnRegister();
    MarkRenderStateDirty();
}


void UEvercoastGaussianSplatCSRendererComp::OnUnregister()
{
    Super::OnUnregister();
}

