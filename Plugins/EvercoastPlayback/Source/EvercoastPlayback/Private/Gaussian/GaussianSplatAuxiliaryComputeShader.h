#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "UnrealEngineCompatibility.h"
#include "RenderGraphUtils.h" // need this to BEGIN_SHADER_PARAMETER_STRUCT

#if PLATFORM_WINDOWS

class FDuplicateGaussianSplatWithKeysCS : public FGlobalShader
{
    DECLARE_SHADER_TYPE(FDuplicateGaussianSplatWithKeysCS, Global)

    SHADER_USE_PARAMETER_STRUCT(FDuplicateGaussianSplatWithKeysCS, FGlobalShader)
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER(uint32_t, SplatCount)
        SHADER_PARAMETER(FVector4f, VecScreenParams)
        SHADER_PARAMETER_SRV(StructuredBuffer<SplatView>, TheSplatViewData)
        SHADER_PARAMETER_SRV(StructuredBuffer<uint32_t>, TileConjugateSplatOffsetBuffer)
        SHADER_PARAMETER_UAV(RWStructuredBuffer<uint32_t>, TileConjugateSplatKeys)
        SHADER_PARAMETER_UAV(RWStructuredBuffer<uint32_t>, TileConjugateSplatValues)
    END_SHADER_PARAMETER_STRUCT()
public:
    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

class FIdentifyGaussianSplatRangesCS : public FGlobalShader
{
    DECLARE_SHADER_TYPE(FIdentifyGaussianSplatRangesCS, Global)

    SHADER_USE_PARAMETER_STRUCT(FIdentifyGaussianSplatRangesCS, FGlobalShader)
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER(uint32_t, NumTileConjugateSplat)
        SHADER_PARAMETER_SRV(StructuredBuffer<uint32_t>, TileConjugateSplatKeysSorted)
        SHADER_PARAMETER_UAV(RWStructuredBuffer<FUintVector2>, TileToSplatRanges)
    END_SHADER_PARAMETER_STRUCT()

public:
    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

#endif