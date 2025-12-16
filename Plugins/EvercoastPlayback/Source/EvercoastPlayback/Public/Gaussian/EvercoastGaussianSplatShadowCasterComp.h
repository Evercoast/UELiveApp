#pragma once

#include "CoreMinimal.h"
#include <memory>
#include "Gaussian/EvercoastGaussianSplatCSRendererComp.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "GlobalShader.h"
#include "UniformBuffer.h"
#include "ShaderParameterUtils.h"
#include "EvercoastStreamingDataUploader.h"
#include "EvercoastGaussianSplatShadowCasterComp.generated.h"

UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class EVERCOASTPLAYBACK_API UEvercoastGaussianSplatShadowCasterComp : public UEvercoastGaussianSplatCSRendererComp
{
    GENERATED_BODY()

public:
    UEvercoastGaussianSplatShadowCasterComp(const FObjectInitializer& ObjectInitializer);

    // ~ Begin UPrimitiveComponent Interface.
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    // ~ End UPrimitiveComponent Interface.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetShadowDecimate, meta = (UIMin = "0.0", UIMax = "1.0"), Category = "Rendering")
    float ShadowDecimate = 0.75f;

    UFUNCTION(BlueprintSetter)
    void SetShadowDecimate(float InSplatDecimate);

    virtual void SetRendererType(EGaussianSplatRendererType rendererType) override;

    virtual void SetEnableTileRendererDepthWrite(bool enableDepthWrite) override;

};
