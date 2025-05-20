#include "Gaussian/EvercoastGaussianSplatComputeComponent.h"
#include "Gaussian/EvercoastGaussianSplatComputeUploader.h"
#include "Gaussian/EvercoastGaussianSplatDecoder.h"
#include "Gaussian/GaussianSplatComputeShader.h"
#include "Gaussian/EvercoastGaussianSplatSceneProxy.h"
#include "RHICommandList.h"
#include "RHIUtilities.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "RenderGraphBuilder.h"
#include "Engine/Engine.h"
#include "MaterialDomain.h"


UEvercoastGaussianSplatComputeComponent::UEvercoastGaussianSplatComputeComponent(const FObjectInitializer& ObjectInitializer) :
    Super(ObjectInitializer),
    m_dirtyMark(true)
{
    CastShadow = false; // HACKHACK: move this to UEvercoastGaussianSplatShadowCasterComponent
    PrimaryComponentTick.bCanEverTick = true;
    bTickInEditor = true; // need this to tick and subsequently call MarkRenderTransformDirty()
    bUseAttachParentBound = false;


    m_dataUploader = std::make_shared<EvercoastGaussianSplatComputeUploader>(this);
}

std::shared_ptr<IEvercoastStreamingDataUploader> UEvercoastGaussianSplatComputeComponent::GetDataUploader() const
{
    return m_dataUploader;
}



void UEvercoastGaussianSplatComputeComponent::MarkDirty()
{
    m_dirtyMark = true;
}

void UEvercoastGaussianSplatComputeComponent::SetGaussianSplatMaterial(UMaterialInterface* newMaterial)
{
    if (GaussianSplatMaterial != newMaterial)
    {
        GaussianSplatMaterial = newMaterial;

        RecreatedDynamicMaterialFromStaticMaterial();

        MarkRenderStateDirty();

        if (SceneProxy)
        {
            ((FEvercoastGaussianSplatSceneProxy*)SceneProxy)->ResetMaterial(GaussianSplatMaterialDynamic);
        }
    }
}

/*
void UEvercoastGaussianSplatComputeComponent::SetReconstructOnTickOnly(bool newValue)
{
    bReconstructOnTickOnly = newValue;

    if (SceneProxy)
    {
        ((FEvercoastGaussianSplatSceneProxy*)SceneProxy)->bPerformLateComputeShaderSplatRecon = bReconstructOnTickOnly;
    }
}
*/

FPrimitiveSceneProxy* UEvercoastGaussianSplatComputeComponent::CreateSceneProxy()
{
    if (!SceneProxy)
    {
        MarkDirty();

        m_dataUploader->MarkDataDirty();

        RecreatedDynamicMaterialFromStaticMaterial();
        FEvercoastGaussianSplatSceneProxy* newSceneProxy = new FEvercoastGaussianSplatSceneProxy(this, GetCreatedDynamicMaterial());

        //newSceneProxy->bPerformLateComputeShaderSplatRecon = bReconstructOnTickOnly;

        ENQUEUE_RENDER_COMMAND(FEvercoastGaussianDataUpdate)(
            [sceneProxy = newSceneProxy, encodedSplatData = GetRetainedEncodedSplatData()](FRHICommandListImmediate& RHICmdList)
            {
                sceneProxy->SetEncodedGaussianSplat_RenderThread(RHICmdList, encodedSplatData);
            });
        return newSceneProxy;
    }

    return SceneProxy;
}

int32 UEvercoastGaussianSplatComputeComponent::GetNumMaterials() const
{
    if (GaussianSplatMaterial)
        return 1;

    return 0;
}

UMaterialInterface* UEvercoastGaussianSplatComputeComponent::GetMaterial(int32 ElementIndex) const
{
    if (GaussianSplatMaterial)
        return GaussianSplatMaterial; // need to return the "vanilla" material to be able to create dynamic material instance

    return UMaterial::GetDefaultMaterial(MD_Surface);
}

void UEvercoastGaussianSplatComputeComponent::GetUsedMaterials(TArray< UMaterialInterface* >& OutMaterials, bool bGetDebugMaterials) const
{
    if (GaussianSplatMaterialDynamic)
        OutMaterials.Add(GaussianSplatMaterialDynamic);
}



void UEvercoastGaussianSplatComputeComponent::SetGaussianSplatData(std::shared_ptr<const EvercoastGaussianSplatPassthroughResult> splatData)
{
    // Retain splat passthrough result
    m_retainedEncodedSplatData = splatData;

    FEvercoastGaussianSplatSceneProxy* sceneProxy = (FEvercoastGaussianSplatSceneProxy*)(this->SceneProxy);

    ENQUEUE_RENDER_COMMAND(FEvercoastGaussianSplatUpload)(
        [sceneProxy, encodedSplatData = m_retainedEncodedSplatData](FRHICommandListImmediate& RHICmdList)
        {
            sceneProxy->SetEncodedGaussianSplat_RenderThread(RHICmdList, encodedSplatData);
        });
}



FBoxSphereBounds UEvercoastGaussianSplatComputeComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    FBoxSphereBounds bounds = CalcLocalBounds();

    return bounds.TransformBy(LocalToWorld);
}

FBoxSphereBounds UEvercoastGaussianSplatComputeComponent::CalcLocalBounds() const
{
    if (SceneProxy)
    {
        FEvercoastGaussianSplatSceneProxy* gaussianSceneProxy = static_cast<FEvercoastGaussianSplatSceneProxy*>(SceneProxy);
        gaussianSceneProxy->GetLocalBounds();
    }

    return FEvercoastGaussianSplatSceneProxy::GetDefaultLocalBounds();
}

#if WITH_EDITOR
void UEvercoastGaussianSplatComputeComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    SceneProxy = nullptr;
    
    MarkRenderStateDirty(); // recreate sceneproxy by end of frame

    Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UEvercoastGaussianSplatComputeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (m_dataUploader->IsDataDirty())
    {
        m_dataUploader->ForceUpload();
    }

    /*
    if (SceneProxy && bReconstructOnTickOnly)
    {
        ((FEvercoastGaussianSplatSceneProxy*)SceneProxy)->PerformLateComputeShaderSplatRecon();
    }
    */

    if (m_dirtyMark)
    {
        UpdateBounds();

        MarkRenderTransformDirty();
        m_dirtyMark = false;
    }
}

FMaterialRelevance UEvercoastGaussianSplatComputeComponent::GetMaterialRelevance(ERHIFeatureLevel::Type InFeatureLevel) const
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

UMaterialInstanceDynamic* UEvercoastGaussianSplatComputeComponent::GetCreatedDynamicMaterial() const
{
    return GaussianSplatMaterialDynamic;
}

std::shared_ptr<const EvercoastGaussianSplatPassthroughResult> UEvercoastGaussianSplatComputeComponent::GetRetainedEncodedSplatData() const
{
    return m_retainedEncodedSplatData;
}

void UEvercoastGaussianSplatComputeComponent::RecreatedDynamicMaterialFromStaticMaterial()
{
    if (GaussianSplatMaterial)
        GaussianSplatMaterialDynamic = CreateAndSetMaterialInstanceDynamicFromMaterial(0, GaussianSplatMaterial);
    else
        GaussianSplatMaterialDynamic = CreateAndSetMaterialInstanceDynamicFromMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));
}