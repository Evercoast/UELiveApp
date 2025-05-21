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
#include "EvercoastGaussianSplatComputeComponent.generated.h"


USTRUCT(BlueprintType)
struct FSplatData
{
    GENERATED_BODY()

    FVector Position;
    float Size;
    FLinearColor Color;
    float Depth;
};

class EvercoastGaussianSplatComputeUploader;
class EvercoastGaussianSplatPassthroughResult;
UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class EVERCOASTPLAYBACK_API UEvercoastGaussianSplatComputeComponent : public UPrimitiveComponent
{
    GENERATED_BODY()

public:
    UEvercoastGaussianSplatComputeComponent(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetGaussianSplatMaterial, Category = "Rendering")
    UMaterialInterface* GaussianSplatMaterial;

//    UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetReconstructOnTickOnly, Category = "Rendering")
//    bool bReconstructOnTickOnly;

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
    void SetGaussianSplatData(std::shared_ptr<const EvercoastGaussianSplatPassthroughResult> splatData);

    /** Accesses the scene relevance information for the materials applied to the mesh. Valid from game thread only. */
    FMaterialRelevance GetMaterialRelevance(ERHIFeatureLevel::Type InFeatureLevel) const;
public:
    UFUNCTION(BlueprintSetter)
    void SetGaussianSplatMaterial(UMaterialInterface* newMaterial);

//    UFUNCTION(BlueprintSetter)
//    void SetReconstructOnTickOnly(bool newValue);
protected:
    //~ Begin UActorComponent interface
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
    std::shared_ptr<const EvercoastGaussianSplatPassthroughResult> GetRetainedEncodedSplatData() const;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void OnRegister() override;
    virtual void OnUnregister() override;
    //~ End UActorComponent interface

private:
    std::shared_ptr<const EvercoastGaussianSplatPassthroughResult> m_retainedEncodedSplatData;
    std::shared_ptr<EvercoastGaussianSplatComputeUploader> m_dataUploader;

    bool m_dirtyMark;
};

