#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "UnrealEngineCompatibility.h"



class FGaussianSplatInitSortDataCS : public FGlobalShader
{
    DECLARE_SHADER_TYPE(FGaussianSplatInitSortDataCS, Global)

public:
    FGaussianSplatInitSortDataCS() {}
    FGaussianSplatInitSortDataCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FGlobalShader(Initializer)
    {
        SplatCount.Bind(Initializer.ParameterMap, TEXT("_SplatCount"));
        SortKeyListA.Bind(Initializer.ParameterMap, TEXT("_SortKeyList_A"));
        SortKeyListB.Bind(Initializer.ParameterMap, TEXT("_SortKeyList_B"));
        SortValueListA.Bind(Initializer.ParameterMap, TEXT("_SortValueList_A"));
        SortValueListB.Bind(Initializer.ParameterMap, TEXT("_SortValueList_B"));
    }

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

    void SetupUniforms(FRHICommandList& RHICmdList, uint32_t splatCount);
    void SetupIOBuffers(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIRef InputSortKeyListUAV_A, FUnorderedAccessViewRHIRef InputSortKeyListUAV_B, 
        FUnorderedAccessViewRHIRef InputSortValueListUAV_A, FUnorderedAccessViewRHIRef InputSortValueListUAV_B);
    void UnbindBuffers(FRHICommandList& RHICmdList);

private:
    LAYOUT_FIELD(FShaderParameter, SplatCount);
    LAYOUT_FIELD(FShaderResourceParameter, SortKeyListA);
    LAYOUT_FIELD(FShaderResourceParameter, SortKeyListB);
    LAYOUT_FIELD(FShaderResourceParameter, SortValueListA);
    LAYOUT_FIELD(FShaderResourceParameter, SortValueListB);
};

// TODO: move the LAYOUT_FIELD data into uniform buffer declaration like BEGIN_SHADER_PARAMETER_STRUCT
class FGaussianSplatComputeShader : public FGlobalShader
{
    DECLARE_SHADER_TYPE(FGaussianSplatComputeShader, Global)

public:
    FGaussianSplatComputeShader() {}
    FGaussianSplatComputeShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FGlobalShader(Initializer)
    {
        SplatCount.Bind(Initializer.ParameterMap, TEXT("_SplatCount"));
        SHDegree.Bind(Initializer.ParameterMap, TEXT("_SHDegree"));
        PositionScaling.Bind(Initializer.ParameterMap, TEXT("_PositionScaling"));
        MatrixObjectToWorld.Bind(Initializer.ParameterMap, TEXT("_MatrixObjectToWorld"));
        VecPreViewTranslation.Bind(Initializer.ParameterMap, TEXT("_VecPreViewTranslation"));
        MatrixVP.Bind(Initializer.ParameterMap, TEXT("_MatrixVP"));
        MatrixV.Bind(Initializer.ParameterMap, TEXT("_MatrixV"));
        MatrixP.Bind(Initializer.ParameterMap, TEXT("_MatrixP"));
        VecScreenParams.Bind(Initializer.ParameterMap, TEXT("_VecScreenParams"));
        MatrixClipToWorld.Bind(Initializer.ParameterMap, TEXT("_MatrixClipToWorld"));
        ClipOverride.Bind(Initializer.ParameterMap, TEXT("_ClipOverride"));
        EncodedSplatPos.Bind(Initializer.ParameterMap, TEXT("_EncodedSplatPos"));
        EncodedSplatColA.Bind(Initializer.ParameterMap, TEXT("_EncodedSplatColA"));
        EncodedSplatScale.Bind(Initializer.ParameterMap, TEXT("_EncodedSplatScale"));
        EncodedSplatRotation.Bind(Initializer.ParameterMap, TEXT("_EncodedSplatRotation"));
        OutputBuffer.Bind(Initializer.ParameterMap, TEXT("_SplatViewData"));
        SortKeyList_A.Bind(Initializer.ParameterMap, TEXT("_SortKeyList_A"));
        SortKeyList_B.Bind(Initializer.ParameterMap, TEXT("_SortKeyList_B"));
    }

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

    void SetupTransformsAndUniforms(FRHICommandList& RHICmdList, const FMatrix& ObjectToWorld, const FVector& PreViewTranslation, const FMatrix& ViewProjection, 
        uint32_t splatCount, uint32_t sphericalHarmonicsDegree, float positionScaling,
        const FMatrix& View, const FMatrix& Projection, const FVector4& ScreenParams, const FMatrix& ClipToWorld, bool isShadowPass);
    void SetupIOBuffers(FRHICommandList& RHICmdList, 
        FUnorderedAccessViewRHIRef InputPositionBufferUAV, 
        FUnorderedAccessViewRHIRef InputColourAlphaBufferUAV,
        FUnorderedAccessViewRHIRef InputScaleBufferUAV,
        FUnorderedAccessViewRHIRef InputRotationBufferUAV,
        FUnorderedAccessViewRHIRef OutputBufferUAV,
        FUnorderedAccessViewRHIRef OutputSortKeyListUAV_A, 
        FUnorderedAccessViewRHIRef OutputSortKeyListUAV_B
        );
    void UnbindBuffers(FRHICommandList& RHICmdList);

private:
    LAYOUT_FIELD(FShaderParameter, SplatCount);
    LAYOUT_FIELD(FShaderParameter, SHDegree);
    LAYOUT_FIELD(FShaderParameter, PositionScaling);
    LAYOUT_FIELD(FShaderParameter, MatrixObjectToWorld);
    LAYOUT_FIELD(FShaderParameter, VecPreViewTranslation);
    LAYOUT_FIELD(FShaderParameter, MatrixVP);
    LAYOUT_FIELD(FShaderParameter, MatrixV);
    LAYOUT_FIELD(FShaderParameter, MatrixP);
    LAYOUT_FIELD(FShaderParameter, VecScreenParams);
    LAYOUT_FIELD(FShaderParameter, MatrixClipToWorld);
    LAYOUT_FIELD(FShaderParameter, ClipOverride);
    LAYOUT_FIELD(FShaderResourceParameter, EncodedSplatPos);
    LAYOUT_FIELD(FShaderResourceParameter, EncodedSplatColA);
    LAYOUT_FIELD(FShaderResourceParameter, EncodedSplatScale);
    LAYOUT_FIELD(FShaderResourceParameter, EncodedSplatRotation);
    LAYOUT_FIELD(FShaderResourceParameter, OutputBuffer);
    LAYOUT_FIELD(FShaderResourceParameter, SortKeyList_A);
    LAYOUT_FIELD(FShaderResourceParameter, SortKeyList_B);

};