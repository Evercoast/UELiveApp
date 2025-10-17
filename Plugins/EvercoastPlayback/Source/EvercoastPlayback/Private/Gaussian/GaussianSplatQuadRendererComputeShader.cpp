#include "Gaussian/GaussianSplatQuadRendererComputeShader.h"

#include "DataDrivenShaderPlatformInfo.h"

IMPLEMENT_SHADER_TYPE(, FGaussianSplatQuadRendererPreprocessComputeShader, TEXT("/EvercoastShaders/EvercoastGaussianSplatPreprocess.usf"), TEXT("CSCalcViewDataForQuadRenderer"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FGaussianSplatInitSortDataCS, TEXT("/EvercoastShaders/EvercoastGaussianSplatQuadRendererOnly.usf"), TEXT("CSInitSortData"), SF_Compute);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FGaussianSplatQuadRendererPreprocessComputeShader::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
void FGaussianSplatQuadRendererPreprocessComputeShader::SetupTransformsAndUniforms(FRHIBatchedShaderParameters& BatchedShaderParams, const FMatrix& ObjectToWorld, const FMatrix& WorldToObject,
	uint32_t splatCount, uint32_t sphericalHarmonicsDegree, uint32_t sphericalHarmonicsDimension, float positionScaling,
	const FMatrix& View, const FMatrix& Projection, const FVector& cameraPosWS, const FVector4& ScreenParams,
	bool IsShadowPass, float SplatDecimation, float splatExtraScale, float cov2DSqrtKernelSize,
	bool showSH0, bool showSH1, bool showSH2, bool showSH3)
{
	SetupBaseTransformsAndUniforms(BatchedShaderParams, ObjectToWorld, WorldToObject,
		splatCount, sphericalHarmonicsDegree, sphericalHarmonicsDimension, positionScaling,
		View, Projection, cameraPosWS, ScreenParams, cov2DSqrtKernelSize, showSH0, showSH1, showSH2, showSH3);

	FVector2f clipOverrideDecimation(IsShadowPass ? 1.0f : -1.0f, SplatDecimation);
	SetShaderValue(BatchedShaderParams, ClipOverrideDecimation, clipOverrideDecimation);
	SetShaderValue(BatchedShaderParams, SplatExtraScale, splatExtraScale);

}
void FGaussianSplatQuadRendererPreprocessComputeShader::SetupIOBuffers(FRHIBatchedShaderParameters& BatchedShaderParams,
	FShaderResourceViewRHIRef InputPositionBufferSRV,
	FShaderResourceViewRHIRef InputColourAlphaBufferSRV,
	FShaderResourceViewRHIRef InputScaleBufferSRV,
	FShaderResourceViewRHIRef InputRotationBufferSRV,
	FShaderResourceViewRHIRef InputSHCoeffsBufferSRV,
	FUnorderedAccessViewRHIRef OutputBufferUAV,
	FUnorderedAccessViewRHIRef SortKeyListUAV
)
{
	SetupBaseIOBuffers(BatchedShaderParams, InputPositionBufferSRV, InputColourAlphaBufferSRV, InputScaleBufferSRV,
		InputRotationBufferSRV, InputSHCoeffsBufferSRV, OutputBufferUAV);

	SetUAVParameter(BatchedShaderParams, SortKeyList, SortKeyListUAV);
}

void FGaussianSplatQuadRendererPreprocessComputeShader::UnbindBuffers(FRHIBatchedShaderUnbinds& BatchedUnbinds)
{
	UnbindBaseBuffers(BatchedUnbinds);

	UnsetUAVParameter(BatchedUnbinds, SortKeyList);
}

#else

void FGaussianSplatQuadRendererPreprocessComputeShader::SetupTransformsAndUniforms(FRHICommandList& RHICmdList, const FMatrix& ObjectToWorld, const FMatrix& WorldToObject,
	uint32_t splatCount, uint32_t sphericalHarmonicsDegree, uint32_t sphericalHarmonicsDimension, float positionScaling,
	const FMatrix& View, const FMatrix& Projection, const FVector& cameraPosWS, const FVector4& ScreenParams,
	bool IsShadowPass, float SplatDecimation, float splatExtraScale, float cov2DSqrtKernelSize,
	bool showSH0, bool showSH1, bool showSH2, bool showSH3)
{
	SetupBaseTransformsAndUniforms(RHICmdList, ObjectToWorld, WorldToObject,
		splatCount, sphericalHarmonicsDegree, sphericalHarmonicsDimension, positionScaling,
		View, Projection, cameraPosWS, ScreenParams, cov2DSqrtKernelSize, showSH0, showSH1, showSH2, showSH3);

	FRHIComputeShader* ShaderRHI = RHICmdList.GetBoundComputeShader();

	FVector2f clipOverrideDecimation(IsShadowPass ? 1.0f : -1.0f, SplatDecimation);
	SetShaderValue(RHICmdList, ShaderRHI, ClipOverrideDecimation, clipOverrideDecimation);
	SetShaderValue(RHICmdList, ShaderRHI, SplatExtraScale, splatExtraScale);
}

void FGaussianSplatQuadRendererPreprocessComputeShader::SetupIOBuffers(FRHICommandList& RHICmdList,
	FShaderResourceViewRHIRef InputPositionBufferSRV,
	FShaderResourceViewRHIRef InputColourAlphaBufferSRV,
	FShaderResourceViewRHIRef InputScaleBufferSRV,
	FShaderResourceViewRHIRef InputRotationBufferSRV,
	FShaderResourceViewRHIRef InputSHCoeffsBufferSRV,
	FUnorderedAccessViewRHIRef OutputBufferUAV,
	FUnorderedAccessViewRHIRef SortKeyListUAV)
{
	SetupBaseIOBuffers(RHICmdList, InputPositionBufferSRV, InputColourAlphaBufferSRV, InputScaleBufferSRV,
		InputRotationBufferSRV, InputSHCoeffsBufferSRV, OutputBufferUAV);

	FRHIComputeShader* ShaderRHI = RHICmdList.GetBoundComputeShader();
	SetUAVParameter(RHICmdList, ShaderRHI, SortKeyList, SortKeyListUAV);
}

void FGaussianSplatQuadRendererPreprocessComputeShader::UnbindBuffers(FRHICommandList& RHICmdList)
{
	UnbindBaseBuffers(RHICmdList);

	FRHIComputeShader* ShaderRHI = RHICmdList.GetBoundComputeShader();
	SetUAVParameter(RHICmdList, ShaderRHI, SortKeyList, nullptr);
}
#endif

//////////////////////////////////////////
// FGaussianSplatInitSortDataCS
bool FGaussianSplatInitSortDataCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}
