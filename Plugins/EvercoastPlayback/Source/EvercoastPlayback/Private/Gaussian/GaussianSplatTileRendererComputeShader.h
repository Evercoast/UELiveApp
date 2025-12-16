#pragma once


#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "UnrealEngineCompatibility.h"
#include "Gaussian/GaussianSplatPreprocessComputeShader.h"

#include "RenderGraphUtils.h" // need this to BEGIN_SHADER_PARAMETER_STRUCT

#if PLATFORM_WINDOWS
// This shader class is subclassed from FGaussianSplatPreprocessComputeShader. It has to use LAYOUT_FIELD(), and manuall Bind()/Unbind() shader parameter & resources
class FGaussianSplatTileRendererPreprocessComputeShader : public FGaussianSplatPreprocessComputeShader
{
    DECLARE_SHADER_TYPE(FGaussianSplatTileRendererPreprocessComputeShader, Global)

public:
    FGaussianSplatTileRendererPreprocessComputeShader() {}
    FGaussianSplatTileRendererPreprocessComputeShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FGaussianSplatPreprocessComputeShader(Initializer)
    {
        TilesTouched.Bind(Initializer.ParameterMap, TEXT("_TilesTouched"));
    }

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    void SetupTransformsAndUniforms(FRHIBatchedShaderParameters& BatchedShaderParams, const FMatrix& ObjectToWorld, const FMatrix& WorldToObject,
        uint32_t splatCount, uint32_t sphericalHarmonicsDegree, uint32_t sphericalHarmonicsDimension, float positionScaling,
        const FMatrix& View, const FMatrix& Projection, const FVector& cameraPosWS, const FVector4& ScreenParams, float kernelSize,
        bool showSH0, bool showSH1, bool showSH2, bool showSH3);

    void SetupIOBuffers(FRHIBatchedShaderParameters& BatchedShaderParams,
        FShaderResourceViewRHIRef InputPositionBufferSRV,
        FShaderResourceViewRHIRef InputColourAlphaBufferSRV,
        FShaderResourceViewRHIRef InputScaleBufferSRV,
        FShaderResourceViewRHIRef InputRotationBufferSRV,
        FShaderResourceViewRHIRef InputSHCoeffsBufferSRV,
        FUnorderedAccessViewRHIRef OutputBufferUAV,
        FUnorderedAccessViewRHIRef OutputTilesTouchedUAV
    );

    void UnbindBuffers(FRHIBatchedShaderUnbinds& BatchedUnbinds);

#else
    void SetupTransformsAndUniforms(FRHICommandList& RHICmdList, const FMatrix& ObjectToWorld, const FMatrix& WorldToObject,
        uint32_t splatCount, uint32_t sphericalHarmonicsDegree, uint32_t sphericalHarmonicsDimension, float positionScaling,
        const FMatrix& View, const FMatrix& Projection, const FVector& cameraPosWS, const FVector4& ScreenParams, float kernelSize,
        bool showSH0, bool showSH1, bool showSH2, bool showSH3);

    void SetupIOBuffers(FRHICommandList& RHICmdList,
        FShaderResourceViewRHIRef InputPositionBufferSRV,
        FShaderResourceViewRHIRef InputColourAlphaBufferSRV,
        FShaderResourceViewRHIRef InputScaleBufferSRV,
        FShaderResourceViewRHIRef InputRotationBufferSRV,
        FShaderResourceViewRHIRef InputSHCoeffsBufferSRV,
        FUnorderedAccessViewRHIRef OutputBufferUAV,
        FUnorderedAccessViewRHIRef OutputTilesTouchedUAV
    );

    void UnbindBuffers(FRHICommandList& RHICmdList);
#endif

private:
    LAYOUT_FIELD(FShaderResourceParameter, TilesTouched);
};


class FGaussianSplatTileRendererCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FGaussianSplatTileRendererCS)
    SHADER_USE_PARAMETER_STRUCT(FGaussianSplatTileRendererCS, FGlobalShader)
    // Once you start to use this BEGIN_SHADER_PARAMETER_STRUCT thingy, you cannot
    // 1. start your varialbe with underscore(_)
    // 2. use the name for a variable which also a struct's name (FXC crash)
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32_t, SplatCount)
        SHADER_PARAMETER(uint32_t, NumTileConjugateSplat)
		SHADER_PARAMETER(FVector4f, VecScreenParams)
        SHADER_PARAMETER(FUintVector2, OutputTextureSize)
        SHADER_PARAMETER(FVector4f, DepthOutputThreshold)

		SHADER_PARAMETER_SRV(StructuredBuffer<SplatView>, TheSplatViewData)
        SHADER_PARAMETER_SRV(StructuredBuffer<uint32_t>, TileConjugateSplatValuesSorted)
		SHADER_PARAMETER_SRV(StructuredBuffer<FUintVector2>, TileToSplatRanges)
		SHADER_PARAMETER_UAV(RWTexture2D<FVector4f>, OutputColourTexture)
		SHADER_PARAMETER_UAV(RWTexture2D<float>, OutputDepthTexture)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	uint32_t GetTileSizeX() const
	{
		return 16;
	}

	uint32_t GetTileSizeY() const
	{
		return 16;
	}
};
#endif