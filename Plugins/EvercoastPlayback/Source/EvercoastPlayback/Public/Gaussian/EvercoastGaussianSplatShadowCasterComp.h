#pragma once

#include "CoreMinimal.h"
#include <memory>
#include "Gaussian/EvercoastGaussianSplatComputeComponent.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "GlobalShader.h"
#include "UniformBuffer.h"
#include "ShaderParameterUtils.h"
#include "EvercoastStreamingDataUploader.h"
#include "EvercoastGaussianSplatShadowCasterComp.generated.h"

UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class EVERCOASTPLAYBACK_API UEvercoastGaussianSplatShadowCasterComp : public UEvercoastGaussianSplatComputeComponent
{
    GENERATED_BODY()

public:
    UEvercoastGaussianSplatShadowCasterComp(const FObjectInitializer& ObjectInitializer);

    // ~ Begin UPrimitiveComponent Interface.
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    // ~ End UPrimitiveComponent Interface.

    //~ Begin USceneComponent Interface.
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
    virtual FBoxSphereBounds CalcLocalBounds() const override;
    //~ End USceneComponent Interface.

};
