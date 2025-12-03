#include "Gaussian/GaussianSplatOffscreenQuadRenderer.h"
#include "Gaussian/EvercoastGaussianSplatCSResult.h"
#include "Gaussian/GaussianSplatPreprocessComputeShader.h"
#include "Gaussian/GaussianSplatQuadRendererComputeShader.h"
#include "Gaussian/GaussianSplatOffscreenQuadRendererShader.h"
#include "GPUSort.h"
#include "CommonRenderResources.h"
#include "EngineModule.h"

// For easier version management in this file only
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#define __CreateUAV RHICmdList.CreateUnorderedAccessView
#define __CreateSRV RHICmdList.CreateShaderResourceView
#define __CreateVertexBuffer RHICmdList.CreateVertexBuffer
#else
#define __CreateUAV RHICreateUnorderedAccessView
#define __CreateSRV RHICreateShaderResourceView
#define __CreateVertexBuffer RHICreateVertexBuffer
#endif

extern TGlobalResource<FOffscreenGaussianQuadVertexDeclaration> GOffscreenGaussianQuadVertexDeclaration;
extern TGlobalResource<FOffscreenGaussianQuadVertexBuffer> GOffscreenGaussianQuadVertexBuffer;
extern TGlobalResource<FOffscreenGaussianQuadIndexBuffer> GOffscreenGaussianQuadIndexBuffer;

// TODO: mobile uses FAST version?
#if 0
typedef FOffscreenGaussianQuadFastVS FOffscreenGaussianQuadVS_Chosen;
typedef FOffscreenGaussianQuadFastPS FOffscreenGaussianQuadPS_Chosen;
typedef FOffscreenGaussianQuadBakeDepthFastPS FOffscreenGaussianQuadBakeDepthPS_Chosen;
#else
typedef FOffscreenGaussianQuadVS FOffscreenGaussianQuadVS_Chosen;
typedef FOffscreenGaussianQuadPS FOffscreenGaussianQuadPS_Chosen;
typedef FOffscreenGaussianQuadBakeDepthPS FOffscreenGaussianQuadBakeDepthPS_Chosen;
#endif


FGaussianSplatOffscreenQuadRenderer::FGaussianSplatOffscreenQuadRenderer() :
	m_maxSplats(0),
	m_outputColourRenderTarget(nullptr),
	m_outputColourRenderTargetFinal(nullptr),
	m_outputDepthRenderTarget(nullptr),
	m_depthStencilTarget(nullptr)
{

}

FGaussianSplatOffscreenQuadRenderer::~FGaussianSplatOffscreenQuadRenderer()
{
	Destroy();
}

void FGaussianSplatOffscreenQuadRenderer::Destroy()
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	ReleaseResources();

	m_maxSplats = 0;
}

void FGaussianSplatOffscreenQuadRenderer::SaveInput(const FMatrix& InObjectToWorld, const FMatrix& InView, const FMatrix& InProj, const FVector& InCameraPositionWS,
	const FVector4& InScreenParam, float InExtraSplatScale, float InCov2DSqrtKernelSize,
	bool InShowSH0, bool InShowSH1, bool InShowSH2, bool InShowSH3, const FVector4& InDepthOutputThreshold,
	std::shared_ptr<const EvercoastGaussianSplatCSResult> InGaussianData)
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	//m_savedFrameCounter++;

	SavedEncodedGaussianSplats = InGaussianData;
	FIntPoint expectedOutputRenderTargetSize = FIntPoint(FMath::RoundUpToPowerOfTwo(InScreenParam.X), FMath::RoundUpToPowerOfTwo(InScreenParam.Y));
	if (m_outputColourRenderTarget)
	{
		FIntPoint existingOutputRenderTargetSize = m_outputColourRenderTarget->GetDesc().Extent;
		if (existingOutputRenderTargetSize != expectedOutputRenderTargetSize)
		{
			expectedOutputRenderTargetSize = existingOutputRenderTargetSize;
		}
	}


	SavedOutputRenderTargetUVScale = FVector2f(InScreenParam.X / expectedOutputRenderTargetSize.X,
		InScreenParam.Y / expectedOutputRenderTargetSize.Y);

	SavedObjectToWorld = InObjectToWorld;
	SavedView = InView;
	SavedProj = InProj;
	SavedCameraPositionWS = InCameraPositionWS;
	SavedScreenParam = InScreenParam;
	SavedExtraSplatScale = InExtraSplatScale;
	SavedCov2DSqrtKernelSize = InCov2DSqrtKernelSize;
	SavedShowSH0 = InShowSH0;
	SavedShowSH1 = InShowSH1;
	SavedShowSH2 = InShowSH2;
	SavedShowSH3 = InShowSH3;
	SavedDepthOutputThreshold = InDepthOutputThreshold;
}

bool FGaussianSplatOffscreenQuadRenderer::RunPipelineWithLastSavedInput_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	return RunPipeline_RenderThread(RHICmdList, SavedObjectToWorld, SavedView, SavedProj, SavedCameraPositionWS, SavedScreenParam, SavedExtraSplatScale, SavedCov2DSqrtKernelSize, SavedShowSH0, SavedShowSH1, SavedShowSH2, SavedShowSH3, SavedDepthOutputThreshold, SavedEncodedGaussianSplats);
}

void FGaussianSplatOffscreenQuadRenderer::ReserveResources(uint32_t splatCount, const FVector4& InScreenParam)
{
	if (m_maxSplats < splatCount)
	{
		m_maxSplats = FMath::RoundUpToPowerOfTwo(splatCount);

		// Release old buffers,
		m_encodedSplatPositionBuffer.SafeRelease();
		m_encodedSplatColourAlphaBuffer.SafeRelease();
		m_encodedSplatScaleBuffer.SafeRelease();
		m_encodedSplatRotationBuffer.SafeRelease();
		m_encodedSplatSHCoeffsBuffer.SafeRelease();
		m_splatViewBuffer.SafeRelease();

		// Release old UAVs/SRVs
		m_encodedSplatPositionSRV.SafeRelease();
		m_encodedSplatColourAlphaSRV.SafeRelease();
		m_encodedSplatScaleSRV.SafeRelease();
		m_encodedSplatRotationSRV.SafeRelease();
		m_encodedSplatSHCoeffsSRV.SafeRelease();
		m_splatViewSRV.SafeRelease();
		m_splatViewUAV.SafeRelease();

		FRHICommandListBase& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		// Sorting buffers
		for (uint32_t i = 0; i < GPU_SORT_BUFFER_COUNT; ++i)
		{
			FRHIResourceCreateInfo sortKeyListCreateInfo(*FString::Printf(TEXT("SortKeyListBuffer%d"), i));
			FRHIResourceCreateInfo sortValueListCreateInfo(*FString::Printf(TEXT("SortValueListBuffer%d"), i));

			m_sortKeyListBuffer[i] = __CreateVertexBuffer(
				sizeof(uint32_t) * m_maxSplats,
				BUF_ShaderResource | BUF_UnorderedAccess, // NO ByteAddressBuffer or StructuredBuffer
				sortKeyListCreateInfo
			);

			m_sortValueListBuffer[i] = __CreateVertexBuffer(
				sizeof(uint32_t) * m_maxSplats,
				BUF_ShaderResource | BUF_UnorderedAccess, // NO ByteAddressBuffer or StructuredBuffer
				sortValueListCreateInfo
			);
		}

		// "Final" copy buffer, as byte address buffer, because vertex factory cannot reads raw buffer(as they are treated like textures in some APIs)
		FRHIResourceCreateInfo sortValueFinalCreateInfo(TEXT("SortValueFinal"));
		m_sortValueFinalBuffer = __CreateVertexBuffer(
			sizeof(uint32_t) * m_maxSplats,
			BUF_ShaderResource | BUF_UnorderedAccess | BUF_ByteAddressBuffer,
			sortValueFinalCreateInfo
		);

		// Position buffer
		FRHIResourceCreateInfo EncodedSplatPositionCreateInfo(TEXT("EncodedSplatPositionBuffer"));
		m_encodedSplatPositionBuffer = RHICmdList.CreateBuffer(
			sizeof(EncodedSplatVector3) * m_maxSplats,
			BUF_ShaderResource | BUF_StructuredBuffer,
			sizeof(EncodedSplatVector3),
			ERHIAccess::SRVMask,
			EncodedSplatPositionCreateInfo
		);

		// Colour + alpha buffer
		FRHIResourceCreateInfo EncodedSplatColourAlphaCreateInfo(TEXT("EncodedSplatColourAlphaBuffer"));
		m_encodedSplatColourAlphaBuffer = RHICmdList.CreateBuffer(
			sizeof(EncodedSplatColourAlpha) * m_maxSplats,
			BUF_ShaderResource | BUF_StructuredBuffer,
			sizeof(EncodedSplatColourAlpha),
			ERHIAccess::SRVMask,
			EncodedSplatColourAlphaCreateInfo
		);

		// Scale buffer
		FRHIResourceCreateInfo EncodedSplatScaleCreationInfo(TEXT("EncodedSplatScaleBuffer"));
		m_encodedSplatScaleBuffer = RHICmdList.CreateBuffer(
			sizeof(EncodedSplatScale) * m_maxSplats,
			BUF_ShaderResource | BUF_StructuredBuffer,
			sizeof(EncodedSplatScale),
			ERHIAccess::SRVMask,
			EncodedSplatScaleCreationInfo
		);

		// Rotation buffer
		FRHIResourceCreateInfo EncodedSplatRotationCreationInfo(TEXT("EncodedSplatRotationBuffer"));
		m_encodedSplatRotationBuffer = RHICmdList.CreateBuffer(
			sizeof(EncodedSplatRotation) * m_maxSplats,
			BUF_ShaderResource | BUF_StructuredBuffer,
			sizeof(EncodedSplatRotation),
			ERHIAccess::SRVMask,
			EncodedSplatRotationCreationInfo
		);

		// SH Coeffs buffer
		FRHIResourceCreateInfo EncodedSplatSHCoeffsCreationInfo(TEXT("EncodedSplatCoeffsBuffer"));
		m_encodedSplatSHCoeffsBuffer = RHICmdList.CreateBuffer(
			sizeof(EncodedSplat3DegreeSHCoeffs) * m_maxSplats,
			BUF_ShaderResource | BUF_StructuredBuffer,
			sizeof(EncodedSplat3DegreeSHCoeffs),
			ERHIAccess::SRVMask,
			EncodedSplatSHCoeffsCreationInfo
		);

		FRHIResourceCreateInfo SplatViewCreationInfo(TEXT("SplatViewBuffer"));
		// SplatView buffer
		m_splatViewBuffer = RHICmdList.CreateBuffer(
			sizeof(SplatView) * m_maxSplats,
			BUF_ShaderResource | BUF_UnorderedAccess | BUF_StructuredBuffer,
			sizeof(SplatView),
			ERHIAccess::UAVMask,
			SplatViewCreationInfo
		);

		// Create UAV/SRV
		for (uint32_t i = 0; i < GPU_SORT_BUFFER_COUNT; ++i)
		{
			// Create UAV and SRV like RunGPUSortTest()
			m_sortKeyListSRV[i] = __CreateSRV(m_sortKeyListBuffer[i], sizeof(uint32), PF_R32_UINT);
			m_sortKeyListUAV[i] = __CreateUAV(m_sortKeyListBuffer[i], PF_R32_UINT);
			m_sortValueListSRV[i] = __CreateSRV(m_sortValueListBuffer[i], sizeof(uint32), PF_R32_UINT);
			m_sortValueListUAV[i] = __CreateUAV(m_sortValueListBuffer[i], PF_R32_UINT);
		}

		m_sortValueFinalSRV = __CreateSRV(m_sortValueFinalBuffer, sizeof(uint32), PF_R32_UINT);
		m_sortValueFinalUAV = __CreateUAV(m_sortValueFinalBuffer, PF_R32_UINT);

		m_encodedSplatPositionSRV = __CreateSRV(m_encodedSplatPositionBuffer);
		m_encodedSplatColourAlphaSRV = __CreateSRV(m_encodedSplatColourAlphaBuffer);
		m_encodedSplatScaleSRV = __CreateSRV(m_encodedSplatScaleBuffer);
		m_encodedSplatRotationSRV = __CreateSRV(m_encodedSplatRotationBuffer);
		m_encodedSplatSHCoeffsSRV = __CreateSRV(m_encodedSplatSHCoeffsBuffer);

		m_splatViewUAV = __CreateUAV(m_splatViewBuffer, false, false);;
		m_splatViewSRV = __CreateSRV(m_splatViewBuffer);
	}


	if (!m_outputColourRenderTarget || m_outputColourRenderTarget->GetSizeX() < InScreenParam.X ||
		m_outputColourRenderTarget->GetSizeY() < InScreenParam.Y)
	{
		m_outputColourRenderTarget.SafeRelease();

		FRHITextureCreateDesc ColourTexDesc = FRHITextureCreateDesc::Create2D(TEXT("Splat Output Colour"), FMath::RoundUpToPowerOfTwo(InScreenParam.X), FMath::RoundUpToPowerOfTwo(InScreenParam.Y), EPixelFormat::PF_FloatRGBA);
		ColourTexDesc.AddFlags(ETextureCreateFlags::SRGB | ETextureCreateFlags::RenderTargetable);
		m_outputColourRenderTarget = RHICreateTexture(ColourTexDesc);
	}

	// Create a matching final colour output texture
	if (!m_outputColourRenderTargetFinal || m_outputColourRenderTargetFinal->GetSizeXY() != m_outputColourRenderTarget->GetSizeXY())
	{
		m_outputColourRenderTargetFinal.SafeRelease();

		FRHITextureCreateDesc ColourTexDesc2 = FRHITextureCreateDesc::Create2D(TEXT("Splat Output Colour Final"), m_outputColourRenderTarget->GetSizeX(), m_outputColourRenderTarget->GetSizeY(), EPixelFormat::PF_FloatRGBA);
		ColourTexDesc2.AddFlags(ETextureCreateFlags::SRGB | ETextureCreateFlags::RenderTargetable);
		m_outputColourRenderTargetFinal = RHICreateTexture(ColourTexDesc2);
	}

	if (!m_outputDepthRenderTarget || m_outputDepthRenderTarget->GetSizeX() < InScreenParam.X ||
		m_outputDepthRenderTarget->GetSizeY() < InScreenParam.Y)
	{
		m_outputDepthRenderTarget.SafeRelease();

		// Since we'll need to bake the "colour" of the depth with alpha blending we will need the full 4-channel texture format
		FRHITextureCreateDesc FloatDepthTexDesc = FRHITextureCreateDesc::Create2D(TEXT("Splat Output Float Depth"), FMath::RoundUpToPowerOfTwo(InScreenParam.X), FMath::RoundUpToPowerOfTwo(InScreenParam.Y), EPixelFormat::PF_FloatRGBA);
		FloatDepthTexDesc.AddFlags(ETextureCreateFlags::SRGB | ETextureCreateFlags::RenderTargetable);
		m_outputDepthRenderTarget = RHICreateTexture(FloatDepthTexDesc);
	}

	if (!m_depthStencilTarget || m_depthStencilTarget->GetSizeXY() != m_outputDepthRenderTarget->GetSizeXY())
	{
		m_depthStencilTarget.SafeRelease();

		FRHITextureCreateDesc DepthStencilTexDesc = FRHITextureCreateDesc::Create2D(TEXT("Splat Output Depth Stencil"), m_outputDepthRenderTarget->GetSizeX(), m_outputDepthRenderTarget->GetSizeY(), EPixelFormat::PF_DepthStencil)
			.SetNumMips(1).SetNumSamples(1)
			.SetFlags(ETextureCreateFlags::DepthStencilTargetable | ETextureCreateFlags::Dynamic)
			.SetClearValue(FClearValueBinding(0, 1));
		m_depthStencilTarget = RHICreateTexture(DepthStencilTexDesc);
	}
}

void FGaussianSplatOffscreenQuadRenderer::ReleaseResources()
{
	// Release old UAVs/SRVs
	m_sortValueFinalSRV.SafeRelease();
	m_sortValueFinalUAV.SafeRelease();

	for (uint32_t i = 0; i < 2; ++i)
	{
		if (m_sortKeyListUAV[i] != nullptr)
			m_sortKeyListUAV[i].SafeRelease();
		if (m_sortKeyListSRV[i] != nullptr)
			m_sortKeyListSRV[i].SafeRelease();
		if (m_sortValueListUAV[i] != nullptr)
			m_sortValueListUAV[i].SafeRelease();
		if (m_sortValueListSRV[i] != nullptr)
			m_sortValueListSRV[i].SafeRelease();
	}

	if (m_encodedSplatPositionSRV)
	{
		m_encodedSplatPositionSRV.SafeRelease();
		m_encodedSplatPositionSRV = nullptr;
	}

	if (m_encodedSplatColourAlphaSRV)
	{
		m_encodedSplatColourAlphaSRV.SafeRelease();
		m_encodedSplatColourAlphaSRV = nullptr;
	}

	if (m_encodedSplatScaleSRV)
	{
		m_encodedSplatScaleSRV.SafeRelease();
		m_encodedSplatScaleSRV = nullptr;
	}

	if (m_encodedSplatRotationSRV)
	{
		m_encodedSplatRotationSRV.SafeRelease();
		m_encodedSplatRotationSRV = nullptr;
	}

	if (m_encodedSplatSHCoeffsSRV)
	{
		m_encodedSplatSHCoeffsSRV.SafeRelease();
		m_encodedSplatSHCoeffsSRV = nullptr;
	}

	if (m_splatViewSRV)
	{
		m_splatViewSRV.SafeRelease();
		m_splatViewSRV = nullptr;
	}

	if (m_splatViewUAV)
	{
		m_splatViewUAV.SafeRelease();
		m_splatViewUAV = nullptr;
	}

	// Release old buffers,
	for (uint32_t i = 0; i < GPU_SORT_BUFFER_COUNT; ++i)
	{
		if (m_sortKeyListBuffer[i] != nullptr)
			m_sortKeyListBuffer[i].SafeRelease();
		if (m_sortValueListBuffer[i] != nullptr)
			m_sortValueListBuffer[i].SafeRelease();
	}
	m_sortValueFinalBuffer.SafeRelease();

	if (m_encodedSplatPositionBuffer)
	{
		m_encodedSplatPositionBuffer.SafeRelease();
		m_encodedSplatPositionBuffer = nullptr;
	}

	if (m_encodedSplatColourAlphaBuffer)
	{
		m_encodedSplatColourAlphaBuffer.SafeRelease();
		m_encodedSplatColourAlphaBuffer = nullptr;
	}

	if (m_encodedSplatScaleBuffer)
	{
		m_encodedSplatScaleBuffer.SafeRelease();
		m_encodedSplatScaleBuffer = nullptr;
	}

	if (m_encodedSplatRotationBuffer)
	{
		m_encodedSplatRotationBuffer.SafeRelease();
		m_encodedSplatRotationBuffer = nullptr;
	}

	if (m_encodedSplatSHCoeffsBuffer)
	{
		m_encodedSplatSHCoeffsBuffer.SafeRelease();
		m_encodedSplatSHCoeffsBuffer = nullptr;
	}

	if (m_splatViewBuffer)
	{
		m_splatViewBuffer.SafeRelease();
		m_splatViewBuffer = nullptr;
	}

	if (m_outputColourRenderTargetFinal)
	{
		m_outputColourRenderTargetFinal.SafeRelease();
		m_outputColourRenderTargetFinal = nullptr;
	}

	if (m_outputColourRenderTarget)
	{
		m_outputColourRenderTarget.SafeRelease();
		m_outputColourRenderTarget = nullptr;
	}

	if (m_outputDepthRenderTarget)
	{
		m_outputDepthRenderTarget.SafeRelease();
		m_outputDepthRenderTarget = nullptr;
	}

	if (m_depthStencilTarget)
	{
		m_depthStencilTarget.SafeRelease();
		m_depthStencilTarget = nullptr;
	}
}

bool FGaussianSplatOffscreenQuadRenderer::RunPipeline_RenderThread(FRHICommandListImmediate& RHICmdList, const FMatrix& InObjectToWorld, const FMatrix& InView, const FMatrix& InProj, const FVector& InCameraPositionWS, 
	const FVector4& InScreenParam, float InExtraSplatScale, float InCov2DSqrtKernelSize, 
	bool showSH0, bool showSH1, bool showSH2, bool showSH3, const FVector4& InDepthOutputThreshold, std::shared_ptr<const EvercoastGaussianSplatCSResult> InGaussianData)
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	m_encodedGaussianSplats = InGaussianData;
	uint32 numSplats = InGaussianData->pointCount;
	FVector4 currScreenParam = InScreenParam;

	ReserveResources(numSplats, currScreenParam);

	uint32 splatsInView = RunPreprocessStage(RHICmdList, InObjectToWorld, InView, InProj, InCameraPositionWS, numSplats, currScreenParam, InExtraSplatScale, InCov2DSqrtKernelSize, showSH0, showSH1, showSH2, showSH3);
	if (splatsInView > 0)
	{
		RunRenderStage(RHICmdList, numSplats, currScreenParam, InDepthOutputThreshold);
		return true;
	}

	return false;
}

uint32_t FGaussianSplatOffscreenQuadRenderer::RunPreprocessStage(FRHICommandListImmediate& RHICmdList, const FMatrix& InObjectToWorld, const FMatrix& InView, const FMatrix& InProj, const FVector& InCameraPositionWS,
	uint32_t numSplats, const FVector4& InScreenParam, float InExtraSplatScale, float InCov2DSqrtKernelSize, bool InShowSH0, bool InShowSH1, bool InShowSH2, bool InShowSH3)
{
	uint32_t ThreadGroupCount;
	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	// Then decode & calculate splat view data
	// Upload position data
	void* PositionBufferData = RHICmdList.LockBuffer(m_encodedSplatPositionBuffer, 0, sizeof(EncodedSplatVector3) * numSplats, RLM_WriteOnly);
	check(sizeof(EncodedSplatVector3) * numSplats == m_encodedGaussianSplats->packedPositionsSize);
	FMemory::Memcpy(PositionBufferData, m_encodedGaussianSplats->packedPositions, m_encodedGaussianSplats->packedPositionsSize);
	RHICmdList.UnlockBuffer(m_encodedSplatPositionBuffer);

	// Upload colour & alpha data
	void* ColourAlphaBufferData = RHICmdList.LockBuffer(m_encodedSplatColourAlphaBuffer, 0, sizeof(EncodedSplatColourAlpha) * numSplats, RLM_WriteOnly);
	check(sizeof(EncodedSplatColourAlpha) * numSplats == m_encodedGaussianSplats->packedColourAlphasSize);
	FMemory::Memcpy(ColourAlphaBufferData, m_encodedGaussianSplats->packedColourAlphas, m_encodedGaussianSplats->packedColourAlphasSize);
	RHICmdList.UnlockBuffer(m_encodedSplatColourAlphaBuffer);

	// Upload scale data
	void* ScaleBufferData = RHICmdList.LockBuffer(m_encodedSplatScaleBuffer, 0, sizeof(EncodedSplatScale) * numSplats, RLM_WriteOnly);
	check(sizeof(EncodedSplatScale) * numSplats == m_encodedGaussianSplats->packedScalesSize);
	FMemory::Memcpy(ScaleBufferData, m_encodedGaussianSplats->packedScales, m_encodedGaussianSplats->packedScalesSize);
	RHICmdList.UnlockBuffer(m_encodedSplatScaleBuffer);

	// Upload rotation data
	void* RotationBufferData = RHICmdList.LockBuffer(m_encodedSplatRotationBuffer, 0, sizeof(EncodedSplatRotation) * numSplats, RLM_WriteOnly);
	check(sizeof(EncodedSplatRotation) * numSplats == m_encodedGaussianSplats->packedRotationsSize);
	FMemory::Memcpy(RotationBufferData, m_encodedGaussianSplats->packedRotations, m_encodedGaussianSplats->packedRotationsSize);
	RHICmdList.UnlockBuffer(m_encodedSplatRotationBuffer);

	// Upload SH coeffs data
	void* SHCoeffsData = RHICmdList.LockBuffer(m_encodedSplatSHCoeffsBuffer, 0, sizeof(EncodedSplat3DegreeSHCoeffs) * numSplats, RLM_WriteOnly);
	// Allow for situations like SH coeff < 3
	check(sizeof(EncodedSplat3DegreeSHCoeffs) * numSplats >= m_encodedGaussianSplats->packedSHCoeffsSize);
	FMemory::Memcpy(SHCoeffsData, m_encodedGaussianSplats->packedSHCoeffs, m_encodedGaussianSplats->packedSHCoeffsSize);
	RHICmdList.UnlockBuffer(m_encodedSplatSHCoeffsBuffer);

	RHICmdList.Transition(FRHITransitionInfo(m_encodedSplatPositionBuffer, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(m_encodedSplatColourAlphaBuffer, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(m_encodedSplatScaleBuffer, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(m_encodedSplatRotationBuffer, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(m_encodedSplatSHCoeffsBuffer, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(m_splatViewUAV, ERHIAccess::Unknown, ERHIAccess::UAVMask));

	TShaderMapRef<FGaussianSplatQuadRendererPreprocessComputeShader> CalcSplatViewDataCS(ShaderMap);


	SetComputePipelineState(RHICmdList, CalcSplatViewDataCS.GetComputeShader());

	FMatrix WorldToObject = InObjectToWorld.Inverse();

	uint32_t shDim = (m_encodedGaussianSplats->shDegree + 1) * (m_encodedGaussianSplats->shDegree + 1) - 1;
	// Bind UAV
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	FRHIBatchedShaderParameters& BatchedShaderParams2 = RHICmdList.GetScratchShaderParameters();
	CalcSplatViewDataCS->SetupTransformsAndUniforms(BatchedShaderParams2, InObjectToWorld, WorldToObject,
		numSplats,
		m_encodedGaussianSplats->shDegree,
		shDim,
		m_encodedGaussianSplats->positionScalar,
		InView, InProj, InCameraPositionWS, InScreenParam, false, 0.0f, InExtraSplatScale, InCov2DSqrtKernelSize,
		InShowSH0, InShowSH1, InShowSH2, InShowSH3, 0 /* sort from front to back*/
	);
	CalcSplatViewDataCS->SetupIOBuffers(BatchedShaderParams2, m_encodedSplatPositionSRV, m_encodedSplatColourAlphaSRV, m_encodedSplatScaleSRV, m_encodedSplatRotationSRV, m_encodedSplatSHCoeffsSRV, m_splatViewUAV, m_sortKeyListUAV[0]);
	RHICmdList.SetBatchedShaderParameters(CalcSplatViewDataCS.GetComputeShader(), BatchedShaderParams2);

#else
		// Transform data only can get from SceneProxy::GetDynamicMeshElements, so this function is called with up-to-date view data
	CalcSplatViewDataCS->SetupTransformsAndUniforms(RHICmdList, InObjectToWorld, WorldToObject,
		numSplats,
		m_encodedGaussianSplats->shDegree,
		shDim,
		m_encodedGaussianSplats->positionScalar,
		InView, InProj, InCameraPositionWS, InScreenParam, false, 0.0f, InExtraSplatScale, InCov2DSqrtKernelSize,
		InShowSH0, InShowSH1, InShowSH2, InShowSH3, 0 /* sort from front to back, must go with (OneMinusDstAlpha, One) blend state*/
	);
	CalcSplatViewDataCS->SetupIOBuffers(RHICmdList, m_encodedSplatPositionSRV, m_encodedSplatColourAlphaSRV, m_encodedSplatScaleSRV, m_encodedSplatRotationSRV, m_encodedSplatSHCoeffsSRV, m_splatViewUAV, m_sortKeyListUAV[0]);
#endif
	// Dispatch
	ThreadGroupCount = FMath::DivideAndRoundUp<uint32>(numSplats, CALC_VIEW_DATA_THREADS);
	RHICmdList.DispatchComputeShader(ThreadGroupCount, 1, 1);

	// Unbind
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
	if (RHICmdList.NeedsShaderUnbinds())
#endif
	{
		FRHIBatchedShaderUnbinds& BatchedUnbinds = RHICmdList.GetScratchShaderUnbinds();
		CalcSplatViewDataCS->UnbindBuffers(BatchedUnbinds);
		RHICmdList.SetBatchedShaderUnbinds(CalcSplatViewDataCS.GetComputeShader(), BatchedUnbinds);
	}
#else
	CalcSplatViewDataCS->UnbindBuffers(RHICmdList);
#endif

	// FIXME: should return a conservative number of splats falls into the view for culling
	return numSplats;
}

void FGaussianSplatOffscreenQuadRenderer::RunRenderStage(FRHICommandListImmediate& RHICmdList, uint32_t numSplats, const FVector4& currScreenParam, const FVector4& InDepthOutputThreshold)
{

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	uint32 ThreadGroupCount;

	// First initialize sorting data
	{
		RHICmdList.Transition(FRHITransitionInfo(m_sortValueListUAV[0], ERHIAccess::Unknown, ERHIAccess::UAVMask));

		TShaderMapRef<FGaussianSplatInitSortDataCS> InitSortDataCS(ShaderMap);

		SetComputePipelineState(RHICmdList, InitSortDataCS.GetComputeShader());

		// Bind
		FGaussianSplatInitSortDataCS::FParameters ShaderParameters;
		ShaderParameters.SplatCount = numSplats;
		ShaderParameters.SortValueList_A = m_sortValueListUAV[0];
		SetShaderParameters(RHICmdList, InitSortDataCS, InitSortDataCS.GetComputeShader(), ShaderParameters);

		// Dispatch
		ThreadGroupCount = FMath::DivideAndRoundUp<uint32>(numSplats, INIT_SORTING_THREADS);
		RHICmdList.DispatchComputeShader(ThreadGroupCount, 1, 1);

		// Unbind
		UnsetShaderUAVs(RHICmdList, InitSortDataCS, InitSortDataCS.GetComputeShader());
	}

	// Run GPU sort
	// Transition resource for reading (optional depending on next usage)
	RHICmdList.Transition(FRHITransitionInfo(m_splatViewUAV, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(m_sortKeyListUAV[0], ERHIAccess::Unknown, ERHIAccess::UAVMask));
	RHICmdList.Transition(FRHITransitionInfo(m_sortKeyListUAV[1], ERHIAccess::Unknown, ERHIAccess::UAVMask));
	RHICmdList.Transition(FRHITransitionInfo(m_sortValueListUAV[0], ERHIAccess::Unknown, ERHIAccess::UAVMask));
	RHICmdList.Transition(FRHITransitionInfo(m_sortValueListUAV[1], ERHIAccess::Unknown, ERHIAccess::UAVMask));

	FGPUSortBuffers SortBuffers; // fill in buffers
	SortBuffers.RemoteKeySRVs[0] = m_sortKeyListSRV[0];
	SortBuffers.RemoteKeySRVs[1] = m_sortKeyListSRV[1];
	SortBuffers.RemoteValueSRVs[0] = m_sortValueListSRV[0];
	SortBuffers.RemoteValueSRVs[1] = m_sortValueListSRV[1];

	SortBuffers.RemoteKeyUAVs[0] = m_sortKeyListUAV[0];
	SortBuffers.RemoteKeyUAVs[1] = m_sortKeyListUAV[1];
	SortBuffers.RemoteValueUAVs[0] = m_sortValueListUAV[0];
	SortBuffers.RemoteValueUAVs[1] = m_sortValueListUAV[1];


	// Run gpu sorter
	int resultBufferIndex = SortGPUBuffers(RHICmdList, SortBuffers, 0, 0xFFFFFFFF, (int32)numSplats, GMaxRHIFeatureLevel);

	// Sort key list for VF shader to read
	RHICmdList.Transition(FRHITransitionInfo(m_sortValueListUAV[resultBufferIndex], ERHIAccess::UAVMask, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(m_sortKeyListUAV[resultBufferIndex], ERHIAccess::UAVMask, ERHIAccess::SRVMask));

	//////////////////////
	// Copy raw buffer to byte address buffer, because GPUSort on Mac doesn't like byte address buffer or structured buffer
	// And vertex factory doesn't like raw buffer
	{
		RHICmdList.Transition(FRHITransitionInfo(m_sortValueFinalUAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute));

		TShaderMapRef<FGaussianSplatCopySortDataCS> CopySortDataCS(ShaderMap);

		SetComputePipelineState(RHICmdList, CopySortDataCS.GetComputeShader());

		// Bind
		FGaussianSplatCopySortDataCS::FParameters ShaderParameters;
		ShaderParameters.SplatCount = numSplats;
		ShaderParameters.SortValueResult = m_sortValueListSRV[resultBufferIndex];
		ShaderParameters.SortValueFinal = m_sortValueFinalUAV;
		SetShaderParameters(RHICmdList, CopySortDataCS, CopySortDataCS.GetComputeShader(), ShaderParameters);

		// Dispatch
		ThreadGroupCount = FMath::DivideAndRoundUp<uint32>(numSplats, COPY_SORTING_THREADS);
		RHICmdList.DispatchComputeShader(ThreadGroupCount, 1, 1);

		// Unbind
		UnsetShaderUAVs(RHICmdList, CopySortDataCS, CopySortDataCS.GetComputeShader());

		RHICmdList.Transition(FRHITransitionInfo(m_sortValueFinalUAV, ERHIAccess::UAVCompute, ERHIAccess::SRVGraphics));
	}

	// Render splats from front to back, with blend state = OneMinusDstAlpha, One
	// first pass to disable z test and z write, do (OneMinusDstAlpha One) blend and write colour map only
	RHICmdList.Transition(FRHITransitionInfo(m_outputColourRenderTarget, ERHIAccess::Unknown, ERHIAccess::RTV));
	FRHIRenderPassInfo RPInfo_ColourBlendingPass(m_outputColourRenderTarget, ERenderTargetActions::Clear_Store);
	RHICmdList.BeginRenderPass(RPInfo_ColourBlendingPass, TEXT("Gaussian Splat Offscreen Quad Blending"));
	{

		RHICmdList.SetViewport(0, 0, 0.0f, currScreenParam.X, currScreenParam.Y, 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);


		

		
		using TSecretIngredientBlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_InverseDestAlpha, BF_One, BO_Add, BF_InverseDestAlpha, BF_One>;

		//GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.BlendState = TSecretIngredientBlendState::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		TShaderMapRef<FOffscreenGaussianQuadVS_Chosen> VertexShader(ShaderMap);
		TShaderMapRef<FOffscreenGaussianQuadPS_Chosen> PixelShader(ShaderMap);
		

		// vertex declaration
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GOffscreenGaussianQuadVertexDeclaration.VertexDeclarationRHI;
		// vertex shader
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		// pixel shader
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();


		// Set PSO
		const uint32_t StencilRef = 0;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

		// Set vertex shader parameters
		FOffscreenGaussianQuadVS_Chosen::FParameters VSParameters;
		VSParameters.NumSplats = numSplats;
		VSParameters.TheSplatViewData = m_splatViewSRV;
		VSParameters.SortValueList = m_sortValueFinalSRV;
		SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), VSParameters);

		// Set pixel shader parameters
		FOffscreenGaussianQuadPS_Chosen::FParameters PSParameters;
		PSParameters.VecScreenParams = FVector4f(currScreenParam);
		SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PSParameters);
		// set stream source, make sure index buffer rhi is created too
		RHICmdList.SetStreamSource(0, GOffscreenGaussianQuadVertexBuffer.VertexBufferRHI, 0);
		
		// Invoke draw call
		RHICmdList.DrawIndexedPrimitive(
			GOffscreenGaussianQuadIndexBuffer.IndexBufferRHI,
			/*BaseVertexIndex=*/ 0,
			/*MinIndex=*/ 0,
			/*NumVertices=*/ 4,
			/*StartIndex=*/ 0,
			/*NumPrimitives=*/ 2,
			/*NumInstances=*/ numSplats);

	}
	RHICmdList.EndRenderPass();

	// add another full screen pass, input is m_outputColourRenderTarget, to compare the final blended alpha with the alpha cutout threshold, then output the final colour
	// Use that final colour for rendering
	// If this step is omitted, the dark edge with feathered silhouette will appear when viewing close-up

	RHICmdList.Transition(FRHITransitionInfo(m_outputColourRenderTarget, ERHIAccess::RTV, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(m_outputColourRenderTargetFinal, ERHIAccess::Unknown, ERHIAccess::RTV));
	FRHIRenderPassInfo RPInfo_ColourRefinePass(m_outputColourRenderTargetFinal, ERenderTargetActions::Clear_Store);
	RHICmdList.BeginRenderPass(RPInfo_ColourRefinePass, TEXT("Gaussian Splat Offscreen Quad Alpha Cutout"));

	{
		RHICmdList.SetViewport(0, 0, 0.0f, m_outputColourRenderTargetFinal->GetSizeX(), m_outputColourRenderTargetFinal->GetSizeY(), 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		TShaderMapRef<FOffscreenGaussianQuadAlphaCutoutVS> VertexShader(ShaderMap);
		TShaderMapRef<FOffscreenGaussianQuadAlphaCutoutPS> PixelShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

		// set pixel shader color map reference
		FOffscreenGaussianQuadAlphaCutoutPS::FParameters PSParam;
		PSParam.DepthOutputThreshold = FVector4f(InDepthOutputThreshold);
		PSParam.LinearClamp = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PSParam.SplatColorMap = m_outputColourRenderTarget;
		SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PSParam);

		// draw call
		GetRendererModule().DrawRectangle(RHICmdList,
			0, 0,				             // Dest X, Y
			m_outputColourRenderTargetFinal->GetSizeX(),  // Dest Width
			m_outputColourRenderTargetFinal->GetSizeY(),  // Dest Height
			0, 0,                            // Source U, V
			1, 1,                            // Source USize, VSize
			m_outputColourRenderTargetFinal->GetSizeXY(), // Target buffer size
			FIntPoint(1, 1),                 // Source texture size
			VertexShader, EDRF_Default);
	}

	RHICmdList.EndRenderPass();

	RHICmdList.Transition(FRHITransitionInfo(m_outputColourRenderTargetFinal, ERHIAccess::RTV, ERHIAccess::SRVMask));

	// Third pass to enable z test & z write, output clip space depth to m_outputDepthRenderTarget
	// There's seems no way to render a proper image and depth in one pass using MRT 
	RHICmdList.Transition(FRHITransitionInfo(m_outputDepthRenderTarget, ERHIAccess::Unknown, ERHIAccess::RTV));
	FRHIRenderPassInfo RPInfo_DepthBakePass(m_outputDepthRenderTarget, ERenderTargetActions::Clear_Store, m_depthStencilTarget, EDepthStencilTargetActions::ClearDepthStencil_StoreDepthStencil);

	RHICmdList.BeginRenderPass(RPInfo_DepthBakePass, TEXT("Gaussian Splat Offscreen Quad Depth Baking"));
	{
		RHICmdList.SetViewport(0, 0, 0.0f, currScreenParam.X, currScreenParam.Y, 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		using TAlphaBlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>;

		GraphicsPSOInit.BlendState = TAlphaBlendState::GetRHI(); // alpha blend
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		// we don't want to sort the splats from back to front again, flip the z write and z test on
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI(); // z test on + z write on
		GraphicsPSOInit.DepthStencilTargetFormat = m_depthStencilTarget->GetFormat();
		GraphicsPSOInit.DepthStencilAccess = FExclusiveDepthStencil::DepthWrite_StencilNop;
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		TShaderMapRef<FOffscreenGaussianQuadVS_Chosen> VertexShader(ShaderMap);
		TShaderMapRef<FOffscreenGaussianQuadBakeDepthPS_Chosen> PixelShader(ShaderMap);

		// vertex declaration
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GOffscreenGaussianQuadVertexDeclaration.VertexDeclarationRHI;
		// vertex shader
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		// pixel shader
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();


		// Set PSO
		const uint32_t StencilRef = 0;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

		// Set vertex shader parameters
		FOffscreenGaussianQuadVS_Chosen::FParameters VSParameters;
		VSParameters.NumSplats = numSplats;
		VSParameters.TheSplatViewData = m_splatViewSRV;
		VSParameters.SortValueList = m_sortValueFinalSRV;
		SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), VSParameters);

		// Set pixel shader parameters
		FOffscreenGaussianQuadBakeDepthPS_Chosen::FParameters PSParameters;
		PSParameters.SplatColorMap = m_outputColourRenderTargetFinal;
		PSParameters.SplatColorMapUVScale = SavedOutputRenderTargetUVScale;
		PSParameters.DepthOutputThreshold = FVector4f(InDepthOutputThreshold);
		PSParameters.LinearClamp = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PSParameters);

		// set stream source, make sure index buffer rhi is created too
		RHICmdList.SetStreamSource(0, GOffscreenGaussianQuadVertexBuffer.VertexBufferRHI, 0);

		// Invoke draw call
		RHICmdList.DrawIndexedPrimitive(
			GOffscreenGaussianQuadIndexBuffer.IndexBufferRHI,
			/*BaseVertexIndex=*/ 0,
			/*MinIndex=*/ 0,
			/*NumVertices=*/ 4,
			/*StartIndex=*/ 0,
			/*NumPrimitives=*/ 2,
			/*NumInstances=*/ numSplats);
	}
	RHICmdList.EndRenderPass();

	RHICmdList.Transition(FRHITransitionInfo(m_outputDepthRenderTarget, ERHIAccess::RTV, ERHIAccess::SRVMask));

}


FTextureRHIRef FGaussianSplatOffscreenQuadRenderer::GetOutputColourRenderTarget() const
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	return m_outputColourRenderTargetFinal;
}

FTextureRHIRef FGaussianSplatOffscreenQuadRenderer::GetOutputDepthRenderTarget() const
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	return m_outputDepthRenderTarget;
}

FVector2f FGaussianSplatOffscreenQuadRenderer::GetSavedOutputRenderTargetUVScale() const
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	return SavedOutputRenderTargetUVScale;
}

#undef __CreateUAV
#undef __CreateSRV