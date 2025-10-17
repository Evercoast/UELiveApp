#include "Gaussian/GaussianSplatPreprocessComputeShader.h"
#include "DataDrivenShaderPlatformInfo.h"


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IMPLEMENT_SHADER_TYPE(, FGaussianSplatPreprocessComputeShader, TEXT("/EvercoastShaders/EvercoastGaussianSplatPreprocess.usf"), TEXT("CSCalcViewDataBase"), SF_Compute);

bool FGaussianSplatPreprocessComputeShader::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
void FGaussianSplatPreprocessComputeShader::SetupBaseTransformsAndUniforms(FRHIBatchedShaderParameters& BatchedShaderParams, const FMatrix& ObjectToWorld, const FMatrix& WorldToObject,
	uint32_t splatCount, uint32_t sphericalHarmonicsDegree, uint32_t sphericalHarmonicsDimension, float positionScaling,
	const FMatrix& View, const FMatrix& Projection, const FVector& cameraPosWS, const FVector4& ScreenParams, float cov2DSqrtKernelSize,
	bool showSH0, bool showSH1, bool showSH2, bool showSH3)
{
	SetShaderValue(BatchedShaderParams, MatrixObjectToWorld, FMatrix44f(ObjectToWorld));
	SetShaderValue(BatchedShaderParams, MatrixWorldToObject, FMatrix44f(WorldToObject));
	SetShaderValue(BatchedShaderParams, SplatCount, splatCount);
	SetShaderValue(BatchedShaderParams, SHDegree, sphericalHarmonicsDegree);
	SetShaderValue(BatchedShaderParams, SHDim, sphericalHarmonicsDimension);
	SetShaderValue(BatchedShaderParams, PositionScaling, positionScaling);

	SetShaderValue(BatchedShaderParams, MatrixV, FMatrix44f(View));
	SetShaderValue(BatchedShaderParams, MatrixP, FMatrix44f(Projection));
	SetShaderValue(BatchedShaderParams, CameraPosWS, FVector3f(cameraPosWS));
	SetShaderValue(BatchedShaderParams, VecScreenParams, FVector4f(ScreenParams));
	SetShaderValue(BatchedShaderParams, Cov2DSqrtKernelSize, cov2DSqrtKernelSize);
	
	SetShaderValue(BatchedShaderParams, SHRender, FUint32Vector4(showSH0, showSH1, showSH2, showSH3));

}
void FGaussianSplatPreprocessComputeShader::SetupBaseIOBuffers(FRHIBatchedShaderParameters& BatchedShaderParams,
	FShaderResourceViewRHIRef InputPositionBufferSRV,
	FShaderResourceViewRHIRef InputColourAlphaBufferSRV,
	FShaderResourceViewRHIRef InputScaleBufferSRV,
	FShaderResourceViewRHIRef InputRotationBufferSRV,
	FShaderResourceViewRHIRef InputSHCoeffsBufferSRV,
	FUnorderedAccessViewRHIRef OutputBufferUAV
)
{
	SetSRVParameter(BatchedShaderParams, EncodedSplatPos, InputPositionBufferSRV);
	SetSRVParameter(BatchedShaderParams, EncodedSplatColA, InputColourAlphaBufferSRV);
	SetSRVParameter(BatchedShaderParams, EncodedSplatScale, InputScaleBufferSRV);
	SetSRVParameter(BatchedShaderParams, EncodedSplatRotation, InputRotationBufferSRV);
	SetSRVParameter(BatchedShaderParams, EncodedSplatSHCoeffs, InputSHCoeffsBufferSRV);
	SetUAVParameter(BatchedShaderParams, OutputBuffer, OutputBufferUAV);

}

void FGaussianSplatPreprocessComputeShader::UnbindBaseBuffers(FRHIBatchedShaderUnbinds& BatchedUnbinds)
{
	UnsetSRVParameter(BatchedUnbinds, EncodedSplatPos);
	UnsetSRVParameter(BatchedUnbinds, EncodedSplatColA);
	UnsetSRVParameter(BatchedUnbinds, EncodedSplatScale);
	UnsetSRVParameter(BatchedUnbinds, EncodedSplatRotation);
	UnsetSRVParameter(BatchedUnbinds, EncodedSplatSHCoeffs);
	UnsetUAVParameter(BatchedUnbinds, OutputBuffer);
	
}

#else

void FGaussianSplatPreprocessComputeShader::SetupBaseTransformsAndUniforms(FRHICommandList& RHICmdList, const FMatrix& ObjectToWorld, const FMatrix& WorldToObject,
	uint32_t splatCount, uint32_t sphericalHarmonicsDegree, uint32_t sphericalHarmonicsDimension, float positionScaling,
	const FMatrix& View, const FMatrix& Projection, const FVector& cameraPosWS, const FVector4& ScreenParams, float cov2DSqrtKernelSize,
	bool showSH0, bool showSH1, bool showSH2, bool showSH3)
{
	FRHIComputeShader* ShaderRHI = RHICmdList.GetBoundComputeShader();
	SetShaderValue(RHICmdList, ShaderRHI, MatrixObjectToWorld, FMatrix44f(ObjectToWorld));
	SetShaderValue(RHICmdList, ShaderRHI, MatrixWorldToObject, FMatrix44f(WorldToObject));
	SetShaderValue(RHICmdList, ShaderRHI, SplatCount, splatCount);
	SetShaderValue(RHICmdList, ShaderRHI, SHDegree, sphericalHarmonicsDegree);
	SetShaderValue(RHICmdList, ShaderRHI, SHDim, sphericalHarmonicsDimension);
	SetShaderValue(RHICmdList, ShaderRHI, SHRender, FUint32Vector4(showSH0, showSH1, showSH2, showSH3));
	SetShaderValue(RHICmdList, ShaderRHI, PositionScaling, positionScaling);
	
	SetShaderValue(RHICmdList, ShaderRHI, MatrixV, FMatrix44f(View));
	SetShaderValue(RHICmdList, ShaderRHI, MatrixP, FMatrix44f(Projection));
	SetShaderValue(RHICmdList, ShaderRHI, VecScreenParams, FVector4f(ScreenParams));

	SetShaderValue(RHICmdList, ShaderRHI, Cov2DSqrtKernelSize, cov2DSqrtKernelSize);
	SetShaderValue(RHICmdList, ShaderRHI, CameraPosWS, FVector3f(cameraPosWS));
}

void FGaussianSplatPreprocessComputeShader::SetupBaseIOBuffers(FRHICommandList& RHICmdList,
	FShaderResourceViewRHIRef InputPositionBufferSRV,
	FShaderResourceViewRHIRef InputColourAlphaBufferSRV,
	FShaderResourceViewRHIRef InputScaleBufferSRV,
	FShaderResourceViewRHIRef InputRotationBufferSRV,
	FShaderResourceViewRHIRef InputSHCoeffsBufferSRV,
	FUnorderedAccessViewRHIRef OutputBufferUAV)
{
	FRHIComputeShader* ShaderRHI = RHICmdList.GetBoundComputeShader();
	SetSRVParameter(RHICmdList, ShaderRHI, EncodedSplatPos, InputPositionBufferSRV);
	SetSRVParameter(RHICmdList, ShaderRHI, EncodedSplatColA, InputColourAlphaBufferSRV);
	SetSRVParameter(RHICmdList, ShaderRHI, EncodedSplatScale, InputScaleBufferSRV);
	SetSRVParameter(RHICmdList, ShaderRHI, EncodedSplatRotation, InputRotationBufferSRV);
	SetSRVParameter(RHICmdList, ShaderRHI, EncodedSplatSHCoeffs, InputSHCoeffsBufferSRV);
	SetUAVParameter(RHICmdList, ShaderRHI, OutputBuffer, OutputBufferUAV);
}

void FGaussianSplatPreprocessComputeShader::UnbindBaseBuffers(FRHICommandList& RHICmdList)
{
	FRHIComputeShader* ShaderRHI = RHICmdList.GetBoundComputeShader();
	SetSRVParameter(RHICmdList, ShaderRHI, EncodedSplatPos, nullptr);
	SetSRVParameter(RHICmdList, ShaderRHI, EncodedSplatColA, nullptr);
	SetSRVParameter(RHICmdList, ShaderRHI, EncodedSplatScale, nullptr);
	SetSRVParameter(RHICmdList, ShaderRHI, EncodedSplatRotation, nullptr);
	SetSRVParameter(RHICmdList, ShaderRHI, EncodedSplatSHCoeffs, nullptr);
	SetUAVParameter(RHICmdList, ShaderRHI, OutputBuffer, nullptr);
}
#endif