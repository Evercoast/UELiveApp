#include "Gaussian/GaussianSplatTileRendererComputeShader.h"
#include "DataDrivenShaderPlatformInfo.h"

#if PLATFORM_WINDOWS

IMPLEMENT_SHADER_TYPE(, FGaussianSplatTileRendererPreprocessComputeShader, TEXT("/EvercoastShaders/EvercoastGaussianSplatPreprocess.usf"), TEXT("CSCalcViewDataForTileRenderer"), SF_Compute);
//IMPLEMENT_SHADER_TYPE(, FGaussianSplatTileRendererCS, TEXT("/EvercoastShaders/EvercoastGaussianSplatTileRenderer.usf"), TEXT("CSRenderViewDataNoGroupSharedMemory"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FGaussianSplatTileRendererCS, TEXT("/EvercoastShaders/EvercoastGaussianSplatTileRenderer.usf"), TEXT("CSRenderViewData"), SF_Compute);
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FGaussianSplatTileRendererPreprocessComputeShader::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
void FGaussianSplatTileRendererPreprocessComputeShader::SetupTransformsAndUniforms(FRHIBatchedShaderParameters& BatchedShaderParams, const FMatrix& ObjectToWorld, const FMatrix& WorldToObject,
	uint32_t splatCount, uint32_t sphericalHarmonicsDegree, uint32_t sphericalHarmonicsDimension, float positionScaling,
	const FMatrix& View, const FMatrix& Projection, const FVector& cameraPosWS, const FVector4& ScreenParams, float cov2DSqrtKernelSize,
	bool showSH0, bool showSH1, bool showSH2, bool showSH3)
{
	SetupBaseTransformsAndUniforms(BatchedShaderParams, ObjectToWorld, WorldToObject, splatCount, sphericalHarmonicsDegree,
		sphericalHarmonicsDimension, positionScaling, View, Projection, cameraPosWS, ScreenParams, cov2DSqrtKernelSize, showSH0,
		showSH1, showSH2, showSH3);
}
void FGaussianSplatTileRendererPreprocessComputeShader::SetupIOBuffers(FRHIBatchedShaderParameters& BatchedShaderParams,
	FShaderResourceViewRHIRef InputPositionBufferSRV,
	FShaderResourceViewRHIRef InputColourAlphaBufferSRV,
	FShaderResourceViewRHIRef InputScaleBufferSRV,
	FShaderResourceViewRHIRef InputRotationBufferSRV,
	FShaderResourceViewRHIRef InputSHCoeffsBufferSRV,
	FUnorderedAccessViewRHIRef OutputBufferUAV,
	FUnorderedAccessViewRHIRef OutputTilesTouchedUAV)
{
	SetupBaseIOBuffers(BatchedShaderParams, InputPositionBufferSRV, InputColourAlphaBufferSRV,
		InputScaleBufferSRV, InputRotationBufferSRV, InputSHCoeffsBufferSRV, OutputBufferUAV);

	SetUAVParameter(BatchedShaderParams, TilesTouched, OutputTilesTouchedUAV);
}

void FGaussianSplatTileRendererPreprocessComputeShader::UnbindBuffers(FRHIBatchedShaderUnbinds& BatchedUnbinds)
{
	UnbindBaseBuffers(BatchedUnbinds);

	UnsetUAVParameter(BatchedUnbinds, TilesTouched);
}

#else

void FGaussianSplatTileRendererPreprocessComputeShader::SetupTransformsAndUniforms(FRHICommandList& RHICmdList, const FMatrix& ObjectToWorld, const FMatrix& WorldToObject,
	uint32_t splatCount, uint32_t sphericalHarmonicsDegree, uint32_t sphericalHarmonicsDimension, float positionScaling,
	const FMatrix& View, const FMatrix& Projection, const FVector& cameraPosWS, const FVector4& ScreenParams, float cov2DSqrtKernelSize,
	bool showSH0, bool showSH1, bool showSH2, bool showSH3)
{
	SetupBaseTransformsAndUniforms(RHICmdList, ObjectToWorld, WorldToObject, splatCount, sphericalHarmonicsDegree,
		sphericalHarmonicsDimension, positionScaling, View, Projection, cameraPosWS, ScreenParams, cov2DSqrtKernelSize, showSH0,
		showSH1, showSH2, showSH3);
}

void FGaussianSplatTileRendererPreprocessComputeShader::SetupIOBuffers(FRHICommandList& RHICmdList,
	FShaderResourceViewRHIRef InputPositionBufferSRV,
	FShaderResourceViewRHIRef InputColourAlphaBufferSRV,
	FShaderResourceViewRHIRef InputScaleBufferSRV,
	FShaderResourceViewRHIRef InputRotationBufferSRV,
	FShaderResourceViewRHIRef InputSHCoeffsBufferSRV,
	FUnorderedAccessViewRHIRef OutputBufferUAV,
	FUnorderedAccessViewRHIRef OutputTilesTouchedUAV)
{
	SetupBaseIOBuffers(RHICmdList, InputPositionBufferSRV, InputColourAlphaBufferSRV,
		InputScaleBufferSRV, InputRotationBufferSRV, InputSHCoeffsBufferSRV, OutputBufferUAV);

	FRHIComputeShader* ShaderRHI = RHICmdList.GetBoundComputeShader();
	SetUAVParameter(RHICmdList, ShaderRHI, TilesTouched, OutputTilesTouchedUAV);
}

void FGaussianSplatTileRendererPreprocessComputeShader::UnbindBuffers(FRHICommandList& RHICmdList)
{
	UnbindBaseBuffers(RHICmdList);

	FRHIComputeShader* ShaderRHI = RHICmdList.GetBoundComputeShader();
	SetUAVParameter(RHICmdList, ShaderRHI, TilesTouched, nullptr);
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FGaussianSplatTileRendererCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
}

#endif