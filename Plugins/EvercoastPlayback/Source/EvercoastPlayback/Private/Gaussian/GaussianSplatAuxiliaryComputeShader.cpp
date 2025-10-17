#include "Gaussian/GaussianSplatAuxiliaryComputeShader.h"
#include "DataDrivenShaderPlatformInfo.h"

#if PLATFORM_WINDOWS

IMPLEMENT_SHADER_TYPE(, FDuplicateGaussianSplatWithKeysCS, TEXT("/EvercoastShaders/EvercoastGaussianSplatAuxiliary.usf"), TEXT("CSDuplicateWithKeys"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FIdentifyGaussianSplatRangesCS, TEXT("/EvercoastShaders/EvercoastGaussianSplatAuxiliary.usf"), TEXT("CSIdentifyTileRanges"), SF_Compute);

/////////////////////////////////////////
// FDuplicateGaussianSplatWithKeysCS
bool FDuplicateGaussianSplatWithKeysCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}


//////////////////////////////////////////
// FIdentifyGaussianSplatRangesCS
bool FIdentifyGaussianSplatRangesCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

#endif