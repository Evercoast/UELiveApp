#include "Gaussian/GaussianSplatComputeShader.h"
#include "DataDrivenShaderPlatformInfo.h"

IMPLEMENT_SHADER_TYPE(, FGaussianSplatComputeShader, TEXT("/EvercoastShaders/EvercoastGaussianSplatCompute.usf"), TEXT("CSCalcViewData"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FGaussianSplatInitSortDataCS, TEXT("/EvercoastShaders/EvercoastGaussianSplatCompute.usf"), TEXT("CSInitSortData"), SF_Compute);


bool FGaussianSplatInitSortDataCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

void FGaussianSplatInitSortDataCS::SetupUniforms(FRHICommandList& RHICmdList, uint32_t splatCount)
{
	FRHIComputeShader* ShaderRHI = RHICmdList.GetBoundComputeShader();
	SetShaderValue(RHICmdList, ShaderRHI, SplatCount, splatCount);
}

void FGaussianSplatInitSortDataCS::SetupIOBuffers(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIRef InputSortKeyListUAV_A, FUnorderedAccessViewRHIRef InputSortKeyListUAV_B, 
	FUnorderedAccessViewRHIRef InputSortValueListUAV_A, FUnorderedAccessViewRHIRef InputSortValueListUAV_B)
{
	FRHIComputeShader* ShaderRHI = RHICmdList.GetBoundComputeShader();
	SetUAVParameter(RHICmdList, ShaderRHI, SortKeyListA, InputSortKeyListUAV_A);
	SetUAVParameter(RHICmdList, ShaderRHI, SortKeyListB, InputSortKeyListUAV_B);
	SetUAVParameter(RHICmdList, ShaderRHI, SortValueListA, InputSortValueListUAV_A);
	SetUAVParameter(RHICmdList, ShaderRHI, SortValueListB, InputSortValueListUAV_B);
}

void FGaussianSplatInitSortDataCS::UnbindBuffers(FRHICommandList& RHICmdList)
{
	FRHIComputeShader* ShaderRHI = RHICmdList.GetBoundComputeShader();
	SetUAVParameter(RHICmdList, ShaderRHI, SortKeyListA, nullptr);
	SetUAVParameter(RHICmdList, ShaderRHI, SortKeyListB, nullptr);
	SetUAVParameter(RHICmdList, ShaderRHI, SortValueListA, nullptr);
	SetUAVParameter(RHICmdList, ShaderRHI, SortValueListB, nullptr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FGaussianSplatComputeShader::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

void FGaussianSplatComputeShader::SetupTransformsAndUniforms(FRHICommandList& RHICmdList, const FMatrix& ObjectToWorld, const FVector& PreViewTranslation, const FMatrix& ViewProjection,
	uint32_t splatCount, uint32_t sphericalHarmonicsDegree, float positionScaling,
	const FMatrix& View, const FMatrix& Projection, const FVector4& ScreenParams, const FMatrix& ClipToWorld, bool IsShadowPass)
{
	FRHIComputeShader* ShaderRHI = RHICmdList.GetBoundComputeShader();
	SetShaderValue(RHICmdList, ShaderRHI, MatrixObjectToWorld, FMatrix44f(ObjectToWorld));
	SetShaderValue(RHICmdList, ShaderRHI, VecPreViewTranslation, FVector3f(PreViewTranslation));
	SetShaderValue(RHICmdList, ShaderRHI, MatrixVP, FMatrix44f(ViewProjection));
	SetShaderValue(RHICmdList, ShaderRHI, SplatCount, splatCount);
	SetShaderValue(RHICmdList, ShaderRHI, SHDegree, sphericalHarmonicsDegree);
	SetShaderValue(RHICmdList, ShaderRHI, PositionScaling, positionScaling);
	SetShaderValue(RHICmdList, ShaderRHI, MatrixV, FMatrix44f(View));
	SetShaderValue(RHICmdList, ShaderRHI, MatrixP, FMatrix44f(Projection));
	SetShaderValue(RHICmdList, ShaderRHI, VecScreenParams, FVector4f(ScreenParams));
	SetShaderValue(RHICmdList, ShaderRHI, MatrixClipToWorld, FMatrix44f(ClipToWorld));
	SetShaderValue(RHICmdList, ShaderRHI, ClipOverride, IsShadowPass ? 1.0f : -1.0f);
}

void FGaussianSplatComputeShader::SetupIOBuffers(FRHICommandList& RHICmdList, 
	FUnorderedAccessViewRHIRef InputPositionBufferUAV, 
	FUnorderedAccessViewRHIRef InputColourAlphaBufferUAV,
	FUnorderedAccessViewRHIRef InputScaleBufferUAV,
	FUnorderedAccessViewRHIRef InputRotationBufferUAV,
	FUnorderedAccessViewRHIRef OutputBufferUAV,
	FUnorderedAccessViewRHIRef OutputSortKeyListUAV_A,
	FUnorderedAccessViewRHIRef OutputSortKeyListUAV_B)
{
	FRHIComputeShader* ShaderRHI = RHICmdList.GetBoundComputeShader();
	SetUAVParameter(RHICmdList, ShaderRHI, EncodedSplatPos, InputPositionBufferUAV);
	SetUAVParameter(RHICmdList, ShaderRHI, EncodedSplatColA, InputColourAlphaBufferUAV);
	SetUAVParameter(RHICmdList, ShaderRHI, EncodedSplatScale, InputScaleBufferUAV);
	SetUAVParameter(RHICmdList, ShaderRHI, EncodedSplatRotation, InputRotationBufferUAV);
	SetUAVParameter(RHICmdList, ShaderRHI, OutputBuffer, OutputBufferUAV);
	SetUAVParameter(RHICmdList, ShaderRHI, SortKeyList_A, OutputSortKeyListUAV_A);
	SetUAVParameter(RHICmdList, ShaderRHI, SortKeyList_B, OutputSortKeyListUAV_B);
}

void FGaussianSplatComputeShader::UnbindBuffers(FRHICommandList& RHICmdList)
{
	FRHIComputeShader* ShaderRHI = RHICmdList.GetBoundComputeShader();
	SetUAVParameter(RHICmdList, ShaderRHI, EncodedSplatPos, nullptr);
	SetUAVParameter(RHICmdList, ShaderRHI, EncodedSplatColA, nullptr);
	SetUAVParameter(RHICmdList, ShaderRHI, EncodedSplatScale, nullptr);
	SetUAVParameter(RHICmdList, ShaderRHI, EncodedSplatRotation, nullptr);
	SetUAVParameter(RHICmdList, ShaderRHI, OutputBuffer, nullptr);
	SetUAVParameter(RHICmdList, ShaderRHI, SortKeyList_A, nullptr);
	SetUAVParameter(RHICmdList, ShaderRHI, SortKeyList_B, nullptr);

}