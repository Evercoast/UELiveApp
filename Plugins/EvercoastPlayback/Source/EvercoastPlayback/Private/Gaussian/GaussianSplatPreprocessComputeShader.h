#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "UnrealEngineCompatibility.h"



// Base preprocessor compute shader class for both quad and tile renderer
// It has to use LAYOUT_FIELD()
class FGaussianSplatPreprocessComputeShader : public FGlobalShader
{
    DECLARE_SHADER_TYPE(FGaussianSplatPreprocessComputeShader, Global)

public:
    FGaussianSplatPreprocessComputeShader() {}
    FGaussianSplatPreprocessComputeShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FGlobalShader(Initializer)
    {
        SplatCount.Bind(Initializer.ParameterMap, TEXT("_SplatCount"));
        SHDegree.Bind(Initializer.ParameterMap, TEXT("_SHDegree"));
        SHDim.Bind(Initializer.ParameterMap, TEXT("_SHDim"));
        SHRender.Bind(Initializer.ParameterMap, TEXT("_SHRender"));
        PositionScaling.Bind(Initializer.ParameterMap, TEXT("_PositionScaling"));
        MatrixObjectToWorld.Bind(Initializer.ParameterMap, TEXT("_MatrixObjectToWorld"));
        MatrixWorldToObject.Bind(Initializer.ParameterMap, TEXT("_MatrixWorldToObject"));
        MatrixV.Bind(Initializer.ParameterMap, TEXT("_MatrixV"));
        MatrixP.Bind(Initializer.ParameterMap, TEXT("_MatrixP"));
        VecScreenParams.Bind(Initializer.ParameterMap, TEXT("_VecScreenParams"));
        Cov2DSqrtKernelSize.Bind(Initializer.ParameterMap, TEXT("_Cov2DSqrtKernelSize"));
        CameraPosWS.Bind(Initializer.ParameterMap, TEXT("_CameraPosWS"));
        EncodedSplatPos.Bind(Initializer.ParameterMap, TEXT("_EncodedSplatPos"));
        EncodedSplatColA.Bind(Initializer.ParameterMap, TEXT("_EncodedSplatColA"));
        EncodedSplatScale.Bind(Initializer.ParameterMap, TEXT("_EncodedSplatScale"));
        EncodedSplatRotation.Bind(Initializer.ParameterMap, TEXT("_EncodedSplatRotation"));
        EncodedSplatSHCoeffs.Bind(Initializer.ParameterMap, TEXT("_EncodedSplatSHCoeffs"));
        OutputBuffer.Bind(Initializer.ParameterMap, TEXT("_SplatViewData"));
    }

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    void SetupBaseTransformsAndUniforms(FRHIBatchedShaderParameters& BatchedShaderParams, const FMatrix& ObjectToWorld, const FMatrix& WorldToObject,
        uint32_t splatCount, uint32_t sphericalHarmonicsDegree, uint32_t sphericalHarmonicsDimension, float positionScaling,
        const FMatrix& View, const FMatrix& Projection, const FVector& cameraPosWS, const FVector4& ScreenParams, float kernelSize,
        bool showSH0, bool showSH1, bool showSH2, bool showSH3);
    void SetupBaseIOBuffers(FRHIBatchedShaderParameters& BatchedShaderParams,
        FShaderResourceViewRHIRef InputPositionBufferSRV,
        FShaderResourceViewRHIRef InputColourAlphaBufferSRV,
        FShaderResourceViewRHIRef InputScaleBufferSRV,
        FShaderResourceViewRHIRef InputRotationBufferSRV,
        FShaderResourceViewRHIRef InputSHCoeffsBufferSRV,
        FUnorderedAccessViewRHIRef OutputBufferUAV
    );

    void UnbindBaseBuffers(FRHIBatchedShaderUnbinds& BatchedUnbinds);

#else
    void SetupBaseTransformsAndUniforms(FRHICommandList& RHICmdList, const FMatrix& ObjectToWorld, const FMatrix& WorldToObject,
        uint32_t splatCount, uint32_t sphericalHarmonicsDegree, uint32_t sphericalHarmonicsDimension, float positionScaling,
        const FMatrix& View, const FMatrix& Projection, const FVector& cameraPosWS, const FVector4& ScreenParams, float kernelSize,
        bool showSH0, bool showSH1, bool showSH2, bool showSH3);

    void SetupBaseIOBuffers(FRHICommandList& RHICmdList, 
        FShaderResourceViewRHIRef InputPositionBufferSRV, 
        FShaderResourceViewRHIRef InputColourAlphaBufferSRV,
        FShaderResourceViewRHIRef InputScaleBufferSRV,
        FShaderResourceViewRHIRef InputRotationBufferSRV,
        FShaderResourceViewRHIRef InputSHCoeffsBufferSRV,
        FUnorderedAccessViewRHIRef OutputBufferUAV
        );

    void UnbindBaseBuffers(FRHICommandList& RHICmdList);
#endif

private:
    LAYOUT_FIELD(FShaderParameter, SplatCount);
    LAYOUT_FIELD(FShaderParameter, SHDegree);
    LAYOUT_FIELD(FShaderParameter, SHDim);
    LAYOUT_FIELD(FShaderParameter, SHRender);
    LAYOUT_FIELD(FShaderParameter, PositionScaling);
    LAYOUT_FIELD(FShaderParameter, MatrixObjectToWorld);
    LAYOUT_FIELD(FShaderParameter, MatrixWorldToObject);
    LAYOUT_FIELD(FShaderParameter, MatrixV);
    LAYOUT_FIELD(FShaderParameter, MatrixP);
    LAYOUT_FIELD(FShaderParameter, VecScreenParams);
    LAYOUT_FIELD(FShaderParameter, Cov2DSqrtKernelSize);
    LAYOUT_FIELD(FShaderParameter, CameraPosWS);
    LAYOUT_FIELD(FShaderResourceParameter, EncodedSplatPos);
    LAYOUT_FIELD(FShaderResourceParameter, EncodedSplatColA);
    LAYOUT_FIELD(FShaderResourceParameter, EncodedSplatScale);
    LAYOUT_FIELD(FShaderResourceParameter, EncodedSplatRotation);
    LAYOUT_FIELD(FShaderResourceParameter, EncodedSplatSHCoeffs);
    LAYOUT_FIELD(FShaderResourceParameter, OutputBuffer);
};

