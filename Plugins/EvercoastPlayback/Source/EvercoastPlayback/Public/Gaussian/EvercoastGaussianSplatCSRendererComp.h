#pragma once

#include "CoreMinimal.h"
#include <memory>
#include "Components/PrimitiveComponent.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "GlobalShader.h"
#include "UniformBuffer.h"
#include "ShaderParameterUtils.h"
#include "EvercoastStreamingDataUploader.h"
#include "EvercoastGaussianSplatCSRendererComp.generated.h"


USTRUCT(BlueprintType)
struct FSplatData
{
    GENERATED_BODY()

    FVector Position;
    float Size;
    FLinearColor Color;
    float Depth;
};

UENUM(BlueprintType) // Makes the enum available to Blueprints
enum class EGaussianSplatRendererType : uint8 // Use enum class for strong typing and specify underlying type
{
    QUAD_RENDERER UMETA(DisplayName = "Quad Renderer(Fast)"),
    TILE_RENDERER UMETA(DisplayName = "Tile Renderer(Best Accuracy)"),
    OFFSCREEN_QUAD_RENDERER UMETA(DisplayName = "Offscreen Quad Renderer(Good Accuracy & Fast)"),
};

UENUM(BlueprintType)
enum class EGaussianSplatHookStage : uint8
{
    POST_OPAQUE = 0 UMETA(DisplayName = "Post Opaque(Before Translucency)"),
    OVERLAY = 1 UMETA(DisplayName = "Overlay(After Translucency)"),
    POST_TONEMAPPING = 2 UMETA(DisplayName = "Post Tonemapping(After Tonemapping and colour space conversion)"),
};

class EvercoastGaussianSplatCSUploader;
class EvercoastGaussianSplatCSResult;
class UTextureRenderTarget2D;
UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class EVERCOASTPLAYBACK_API UEvercoastGaussianSplatCSRendererComp : public UPrimitiveComponent
{
    GENERATED_BODY()

public:

    UEvercoastGaussianSplatCSRendererComp(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetGaussianSplatMaterial, Category = "Rendering")
    UMaterialInterface* GaussianSplatMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetRendererType, Category = "Rendering")
    EGaussianSplatRendererType rendererType = EGaussianSplatRendererType::OFFSCREEN_QUAD_RENDERER;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetReconstructOnTickOnly, Category = "Rendering")
    bool bReconstructOnTickOnly;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetSplatExtraScale, meta = (EditCondition = "(rendererType == EGaussianSplatRendererType::QUAD_RENDERER || rendererType == EGaussianSplatRendererType::OFFSCREEN_QUAD_RENDERER)", EditConditionHides, UIMin = "0.0", UIMax = "2.0"), Category = "Rendering")
    float SplatExtraScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetCov2DSqrtKernelSize, meta = (UIMin = "0.0", UIMax = "1.0"), Category = "Rendering")
    float Cov2DSqrtKernelSize = 0.3f; // original paper implementation, discussion:  https://github.com/graphdeco-inria/gaussian-splatting/issues/294#issuecomment-1772688093

    UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetShowSphericalHarmonics0, Category = "Rendering")
    bool bShowDiffuseColour = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetShowSphericalHarmonics1, Category = "Rendering")
    bool bShowSphericalHarmonics1Colour = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetShowSphericalHarmonics2, Category = "Rendering")
    bool bShowSphericalHarmonics2Colour = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetShowSphericalHarmonics3, Category = "Rendering")
    bool bShowSphericalHarmonics3Colour = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetTileRendererHookStage, meta = (EditCondition = "rendererType == EGaussianSplatRendererType::TILE_RENDERER || rendererType == EGaussianSplatRendererType::OFFSCREEN_QUAD_RENDERER", EditConditionHides), Category = "Rendering")
    EGaussianSplatHookStage TileRendererHookStage = EGaussianSplatHookStage::POST_OPAQUE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetEnableTileRendererDepthWrite, meta = (EditCondition = "rendererType == EGaussianSplatRendererType::TILE_RENDERER || rendererType == EGaussianSplatRendererType::OFFSCREEN_QUAD_RENDERER", EditConditionHides), Category = "Rendering")
    bool bEnableTileRendererDepthWrite = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetTileRendererAlphaCutoutThreshold, meta = (UIMin = "0.0", UIMax = "1.0", EditCondition = "(rendererType == EGaussianSplatRendererType::TILE_RENDERER || rendererType == EGaussianSplatRendererType::OFFSCREEN_QUAD_RENDERER) && bEnableTileRendererDepthWrite", EditConditionHides), Category = "Rendering")
    float TileRendererAlphaCutoutThreshold = 0.667f;

    // ~ Begin UPrimitiveComponent Interface.
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    virtual int32 GetNumMaterials() const override;
    virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
    virtual void GetUsedMaterials(TArray< UMaterialInterface* >& OutMaterials, bool bGetDebugMaterials) const override;
    // ~ End UPrimitiveComponent Interface.

    //~ Begin USceneComponent Interface.
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
    virtual FBoxSphereBounds CalcLocalBounds() const override;
    //~ End USceneComponent Interface.

    std::shared_ptr<IEvercoastStreamingDataUploader> GetDataUploader() const;
    void MarkDirty();
    void SetGaussianSplatData(std::shared_ptr<const EvercoastGaussianSplatCSResult> splatData);

    /** Accesses the scene relevance information for the materials applied to the mesh. Valid from game thread only. */
    FMaterialRelevance GetMaterialRelevance(ERHIFeatureLevel::Type InFeatureLevel) const;
public:
    UFUNCTION(BlueprintSetter)
    void SetGaussianSplatMaterial(UMaterialInterface* newMaterial);

    UFUNCTION(BlueprintSetter)
    void SetReconstructOnTickOnly(bool newValue);

    UFUNCTION(BlueprintSetter)
    void SetSplatExtraScale(float InSplatExtraScale);

    UFUNCTION(BlueprintSetter)
    void SetCov2DSqrtKernelSize(float InKernelSize);

    UFUNCTION(BlueprintSetter)
    void SetShowSphericalHarmonics0(bool show);

    UFUNCTION(BlueprintSetter)
    void SetShowSphericalHarmonics1(bool show);

    UFUNCTION(BlueprintSetter)
    void SetShowSphericalHarmonics2(bool show);
    
    UFUNCTION(BlueprintSetter)
    void SetShowSphericalHarmonics3(bool show);

    UFUNCTION(BlueprintSetter)
    virtual void SetRendererType(EGaussianSplatRendererType newType);

    UFUNCTION(BlueprintSetter)
    virtual void SetEnableTileRendererDepthWrite(bool enableDepthWrite);

    UFUNCTION(BlueprintSetter)
    void SetTileRendererHookStage(EGaussianSplatHookStage stage);

    UFUNCTION(BlueprintSetter)
    void SetTileRendererAlphaCutoutThreshold(float InAlphaCutout);
    
protected:
    //~ Begin UActorComponent interface
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
    std::shared_ptr<const EvercoastGaussianSplatCSResult> GetRetainedEncodedSplatData() const;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void OnRegister() override;
    virtual void OnUnregister() override;
    //~ End UActorComponent interface

private:
    std::shared_ptr<const EvercoastGaussianSplatCSResult> m_retainedEncodedSplatData;
    std::shared_ptr<EvercoastGaussianSplatCSUploader> m_dataUploader;

    bool m_dirtyMark;

    mutable bool m_materialDirty;

protected:
    UPROPERTY(Transient)
    UMaterialInstanceDynamic* m_materialInstance;

    UMaterialInstanceDynamic* GetNewMaterialInstanceDynamic();
};

