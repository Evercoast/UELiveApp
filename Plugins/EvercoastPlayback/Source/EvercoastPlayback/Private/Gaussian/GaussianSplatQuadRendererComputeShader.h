#pragma once



#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "UnrealEngineCompatibility.h"
#include "Gaussian/GaussianSplatPreprocessComputeShader.h"

// This shader class is subclassed from FGaussianSplatPreprocessComputeShader. It has to use LAYOUT_FIELD() 
class FGaussianSplatQuadRendererPreprocessComputeShader : public FGaussianSplatPreprocessComputeShader
{
    DECLARE_SHADER_TYPE(FGaussianSplatQuadRendererPreprocessComputeShader, Global)
public:
    FGaussianSplatQuadRendererPreprocessComputeShader() {}
    FGaussianSplatQuadRendererPreprocessComputeShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FGaussianSplatPreprocessComputeShader(Initializer)
    {
        ClipOverrideDecimation.Bind(Initializer.ParameterMap, TEXT("_ClipOverrideDecimation"));
        SplatExtraScale.Bind(Initializer.ParameterMap, TEXT("_SplatExtraScale"));
        SortKeyList.Bind(Initializer.ParameterMap, TEXT("_SortKeyList_A"));
    }

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    void SetupTransformsAndUniforms(FRHIBatchedShaderParameters& BatchedShaderParams, const FMatrix& ObjectToWorld, const FMatrix& WorldToObject,
        uint32_t splatCount, uint32_t sphericalHarmonicsDegree, uint32_t sphericalHarmonicsDimension, float positionScaling,
        const FMatrix& View, const FMatrix& Projection, const FVector& cameraPosWS, const FVector4& ScreenParams,
        bool isShadowPass, float splatDecimation, float splatExtraScale, float kernelSize,
        bool showSH0, bool showSH1, bool showSH2, bool showSH3);

    void SetupIOBuffers(FRHIBatchedShaderParameters& BatchedShaderParams,
        FShaderResourceViewRHIRef InputPositionBufferSRV,
        FShaderResourceViewRHIRef InputColourAlphaBufferSRV,
        FShaderResourceViewRHIRef InputScaleBufferSRV,
        FShaderResourceViewRHIRef InputRotationBufferSRV,
        FShaderResourceViewRHIRef InputSHCoeffsBufferSRV,
        FUnorderedAccessViewRHIRef OutputBufferUAV,
        FUnorderedAccessViewRHIRef OutputSortKeyListUAV
    );

    void UnbindBuffers(FRHIBatchedShaderUnbinds& BatchedUnbinds);

#else
    void SetupTransformsAndUniforms(FRHICommandList& RHICmdList, const FMatrix& ObjectToWorld, const FMatrix& WorldToObject,
        uint32_t splatCount, uint32_t sphericalHarmonicsDegree, uint32_t sphericalHarmonicsDimension, float positionScaling,
        const FMatrix& View, const FMatrix& Projection, const FVector& cameraPosWS, const FVector4& ScreenParams,
        bool isShadowPass, float splatDecimation, float splatExtraScale, float kernelSize,
        bool showSH0, bool showSH1, bool showSH2, bool showSH3);
    
    void SetupIOBuffers(FRHICommandList& RHICmdList,
        FShaderResourceViewRHIRef InputPositionBufferSRV,
        FShaderResourceViewRHIRef InputColourAlphaBufferSRV,
        FShaderResourceViewRHIRef InputScaleBufferSRV,
        FShaderResourceViewRHIRef InputRotationBufferSRV,
        FShaderResourceViewRHIRef InputSHCoeffsBufferSRV,
        FUnorderedAccessViewRHIRef OutputBufferUAV,
        FUnorderedAccessViewRHIRef OutputSortKeyListUAV
    );
    void UnbindBuffers(FRHICommandList& RHICmdList);
#endif

private:
    LAYOUT_FIELD(FShaderParameter, ClipOverrideDecimation);
    LAYOUT_FIELD(FShaderParameter, SplatExtraScale);
    LAYOUT_FIELD(FShaderResourceParameter, SortKeyList);
};

class FGaussianSplatInitSortDataCS : public FGlobalShader
{
    DECLARE_SHADER_TYPE(FGaussianSplatInitSortDataCS, Global)
    SHADER_USE_PARAMETER_STRUCT(FGaussianSplatInitSortDataCS, FGlobalShader)
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER(uint32_t, SplatCount)
        SHADER_PARAMETER_UAV(RWStructuredBuffer<uint32_t>, SortValueList_A)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

};
