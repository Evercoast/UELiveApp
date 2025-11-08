#include "Gaussian/GaussianSplatTileRenderer.h"
#include "Gaussian/EvercoastGaussianSplatCSResult.h"
#include "RenderUtils.h"
#include "Gaussian/GaussianSplatPreprocessComputeShader.h"
#include "Gaussian/InclusiveSumComputeShader.h"
#include "Gaussian/GaussianSplatAuxiliaryComputeShader.h"
#include "Gaussian/GaussianSplatTileRendererComputeShader.h"
#include "Gaussian/GaussianSplatComputeShaderConstants.h"
#include "MaterialShared.h"
#include "GPUSort.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EvercoastVoxelDecoder.h" // log define

#if PLATFORM_WINDOWS

// For easier version management in this file only
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#define __CreateUAV RHICmdList.CreateUnorderedAccessView
#define __CreateSRV RHICmdList.CreateShaderResourceView
#else
#define __CreateUAV RHICreateUnorderedAccessView
#define __CreateSRV RHICreateShaderResourceView
#endif


FGaussianSplatTileRenderer::FGaussianSplatTileRenderer() :
	m_maxSplats(0),
	m_maxTileConjugateSplat(0),
	m_currSortResultBufferIndex(0),
	m_numTileX(0),
	m_numTileY(0),
	m_savedFrameCounter(0),
	m_outputColourRenderTarget(nullptr),
	m_outputDepthRenderTarget(nullptr),
	m_outputFrameCounter(0)
	
{
}

FGaussianSplatTileRenderer::~FGaussianSplatTileRenderer()
{
	Destroy();
}


void FGaussianSplatTileRenderer::Destroy()
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	ReleaseSecondStageResources();
	ReleaseFirstStageResources();
	ReleaseViewDependentResources();
	ReleasePermanentResources();

	m_maxSplats = 0;
	m_maxTileConjugateSplat = 0;
	m_currSortResultBufferIndex = 0;
	m_numTileX = 0;
	m_numTileY = 0;
	m_savedFrameCounter = 0;
}

void FGaussianSplatTileRenderer::ReleaseViewDependentResources()
{


	if (m_outputColourRenderTargetUAV)
	{
		m_outputColourRenderTargetUAV.SafeRelease();
		m_outputColourRenderTargetUAV = nullptr;
	}
	if (m_outputDepthRenderTargetUAV)
	{
		m_outputDepthRenderTargetUAV.SafeRelease();
		m_outputDepthRenderTargetUAV = nullptr;
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
}

void FGaussianSplatTileRenderer::ReserveViewDependentResources(const FVector4 InScreenParam)
{


	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	if (!m_outputColourRenderTarget ||
		m_outputColourRenderTarget->GetSizeX() < InScreenParam.X ||
		m_outputColourRenderTarget->GetSizeY() < InScreenParam.Y)
	{
		m_outputColourRenderTarget.SafeRelease();
		m_outputColourRenderTargetUAV.SafeRelease();
		m_outputColourRenderTargetUAV = nullptr;

		FRHITextureCreateDesc ColourTexDesc = FRHITextureCreateDesc::Create2D(TEXT("Splat Output Colour"), FMath::RoundUpToPowerOfTwo(InScreenParam.X), FMath::RoundUpToPowerOfTwo(InScreenParam.Y), EPixelFormat::PF_FloatRGBA);
		ColourTexDesc.AddFlags(ETextureCreateFlags::SRGB | ETextureCreateFlags::UAV);
		m_outputColourRenderTarget = RHICreateTexture(ColourTexDesc);

		UE_LOG(LogTemp, Verbose, TEXT("Resize %s to %dx%d"), ColourTexDesc.DebugName, ColourTexDesc.Extent.X, ColourTexDesc.Extent.Y);
	}
	if (!m_outputDepthRenderTarget ||
		m_outputDepthRenderTarget->GetSizeX() < InScreenParam.X ||
		m_outputDepthRenderTarget->GetSizeY() < InScreenParam.Y)
	{
		m_outputDepthRenderTarget.SafeRelease();
		m_outputDepthRenderTargetUAV.SafeRelease();
		m_outputDepthRenderTargetUAV = nullptr;

		FRHITextureCreateDesc DepthTexDesc = FRHITextureCreateDesc::Create2D(TEXT("Splat Output Depth"), FMath::RoundUpToPowerOfTwo(InScreenParam.X), FMath::RoundUpToPowerOfTwo(InScreenParam.Y), EPixelFormat::PF_R32_FLOAT);
		DepthTexDesc.AddFlags(ETextureCreateFlags::SRGB | ETextureCreateFlags::UAV);
		m_outputDepthRenderTarget = RHICreateTexture(DepthTexDesc);

		UE_LOG(LogTemp, Verbose, TEXT("Resize %s to %dx%d"), DepthTexDesc.DebugName, DepthTexDesc.Extent.X, DepthTexDesc.Extent.Y);
	}


	if (!m_outputColourRenderTargetUAV)
	{
		m_outputColourRenderTargetUAV = __CreateUAV(m_outputColourRenderTarget);
		UE_LOG(LogTemp, Verbose, TEXT("Recreate UAV of %s size: %dx%d"), *m_outputColourRenderTarget->GetName().ToString(), m_outputColourRenderTarget->GetSizeX(), m_outputColourRenderTarget->GetSizeY());
	}

	if (!m_outputDepthRenderTargetUAV)
	{
		m_outputDepthRenderTargetUAV = __CreateUAV(m_outputDepthRenderTarget);
		UE_LOG(LogTemp, Verbose, TEXT("Recreate UAV of %s size: %dx%d"), *m_outputDepthRenderTarget->GetName().ToString(), m_outputDepthRenderTarget->GetSizeX(), m_outputDepthRenderTarget->GetSizeY());
	}
}

void FGaussianSplatTileRenderer::ReleasePermanentResources()
{


	// UAVs
	if (m_intermInclusiveSumGroupSumsUAV)
	{
		m_intermInclusiveSumGroupSumsUAV.SafeRelease();
		m_intermInclusiveSumGroupSumsUAV = nullptr;
	}

	if (m_intermInclusiveSumGroupSumsScanUAV)
	{
		m_intermInclusiveSumGroupSumsScanUAV.SafeRelease();
		m_intermInclusiveSumGroupSumsScanUAV = nullptr;
	}

	// SRVs
	if (m_intermInclusiveSumGroupSumsSRV)
	{
		m_intermInclusiveSumGroupSumsSRV.SafeRelease();
		m_intermInclusiveSumGroupSumsSRV = nullptr;
	}

	if (m_intermInclusiveSumGroupSumsScanSRV)
	{
		m_intermInclusiveSumGroupSumsScanSRV.SafeRelease();
		m_intermInclusiveSumGroupSumsScanSRV = nullptr;
	}

	// Actual buffers
	if (m_intermInclusiveSumGroupSumsBuffer)
	{
		m_intermInclusiveSumGroupSumsBuffer.SafeRelease();
		m_intermInclusiveSumGroupSumsBuffer = nullptr;
	}

	// Inclusive sum intermediate buffer 2
	if (m_intermInclusiveSumGroupSumsScanBuffer)
	{
		m_intermInclusiveSumGroupSumsScanBuffer.SafeRelease();
		m_intermInclusiveSumGroupSumsScanBuffer = nullptr;
	}

}

void FGaussianSplatTileRenderer::ReservePermanentResources()
{


	// Those resources has fixed size so won't change in the life time of tile renderer
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	if (!m_intermInclusiveSumGroupSumsBuffer)
	{
		// Inclusive sum intermediate buffer 1
		FRHIResourceCreateInfo IntermediateInclusiveSumGroupSumsBufferInfo(TEXT("IntermediateInclusiveSumGroupSumsBuffer"));
		m_intermInclusiveSumGroupSumsBuffer = RHICmdList.CreateBuffer(
			sizeof(uint32_t) * INCLUSIVE_SUM_BLOCK_SIZE,
			BUF_ShaderResource | BUF_UnorderedAccess | BUF_StructuredBuffer,
			sizeof(uint32_t),
			ERHIAccess::UAVMask,
			IntermediateInclusiveSumGroupSumsBufferInfo
		);
	}

	// Inclusive sum intermediate buffer 2
	if (!m_intermInclusiveSumGroupSumsScanBuffer)
	{
		FRHIResourceCreateInfo IntermediateInclusiveSumGroupSumsScanBufferInfo(TEXT("IntermediateInclusiveSumGroupSumsScanBuffer"));
		m_intermInclusiveSumGroupSumsScanBuffer = RHICmdList.CreateBuffer(
			sizeof(uint32_t) * INCLUSIVE_SUM_GROUP_SIZE,
			BUF_ShaderResource | BUF_UnorderedAccess | BUF_StructuredBuffer,
			sizeof(uint32_t),
			ERHIAccess::UAVMask,
			IntermediateInclusiveSumGroupSumsScanBufferInfo
		);
	}

	// UAVs
	if (!m_intermInclusiveSumGroupSumsUAV)
	{
		m_intermInclusiveSumGroupSumsUAV = __CreateUAV(m_intermInclusiveSumGroupSumsBuffer, false, false);
	}

	if (!m_intermInclusiveSumGroupSumsSRV)
	{
		m_intermInclusiveSumGroupSumsSRV = __CreateSRV(m_intermInclusiveSumGroupSumsBuffer);
	}

	if (!m_intermInclusiveSumGroupSumsScanUAV)
	{
		m_intermInclusiveSumGroupSumsScanUAV = __CreateUAV(m_intermInclusiveSumGroupSumsScanBuffer, false, false);
	}

	if (!m_intermInclusiveSumGroupSumsScanSRV)
	{
		m_intermInclusiveSumGroupSumsScanSRV = __CreateSRV(m_intermInclusiveSumGroupSumsScanBuffer);
	}

}

void FGaussianSplatTileRenderer::ReleaseFirstStageResources()
{
	// Release old UAVs/SRVs
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

	if (m_splatNumTileTouchedUAV)
	{
		m_splatNumTileTouchedUAV.SafeRelease();
		m_splatNumTileTouchedUAV = nullptr;
	}

	if (m_splatNumTileTouchedSRV)
	{
		m_splatNumTileTouchedSRV.SafeRelease();
		m_splatNumTileTouchedSRV = nullptr;
	}

	if (m_tileConjugateSplatOffsetUAV)
	{
		m_tileConjugateSplatOffsetUAV.SafeRelease();
		m_tileConjugateSplatOffsetUAV = nullptr;
	}

	if (m_tileConjugateSplatOffsetSRV)
	{
		m_tileConjugateSplatOffsetSRV.SafeRelease();
		m_tileConjugateSplatOffsetSRV = nullptr;
	}

	// Release old buffers,
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

	if (m_splatNumTileTouched)
	{
		m_splatNumTileTouched.SafeRelease();
		m_splatNumTileTouched = nullptr;
	}

	if (m_tileConjugateSplatOffset)
	{
		m_tileConjugateSplatOffset.SafeRelease();
		m_tileConjugateSplatOffset = nullptr;
	}

}

void FGaussianSplatTileRenderer::ReserveFirstStageResources(uint32_t splatCount)
{


	// First stage resources are dependent on those two parameters:
	if (m_maxSplats < splatCount)
	{
		
		m_maxSplats = FMath::RoundUpToPowerOfTwo(splatCount);
		UE_LOG(LogTemp, Verbose, TEXT("Reserve 1st stage resources. Thread: %d Splat count = %d, Max Splat count = %d"), FPlatformTLS::GetCurrentThreadId(), splatCount, m_maxSplats);

		// Release old buffers,
		m_encodedSplatPositionBuffer.SafeRelease();
		m_encodedSplatColourAlphaBuffer.SafeRelease();
		m_encodedSplatScaleBuffer.SafeRelease();
		m_encodedSplatRotationBuffer.SafeRelease();
		m_encodedSplatSHCoeffsBuffer.SafeRelease();
		m_splatViewBuffer.SafeRelease();
		m_splatNumTileTouched.SafeRelease();
		m_tileConjugateSplatOffset.SafeRelease();

		// Release old UAVs/SRVs
		m_encodedSplatPositionSRV.SafeRelease();
		m_encodedSplatColourAlphaSRV.SafeRelease();
		m_encodedSplatScaleSRV.SafeRelease();
		m_encodedSplatRotationSRV.SafeRelease();
		m_encodedSplatSHCoeffsSRV.SafeRelease();
		m_splatViewSRV.SafeRelease();
		m_splatViewUAV.SafeRelease();
		m_splatNumTileTouchedUAV.SafeRelease();
		m_splatNumTileTouchedSRV.SafeRelease();
		m_tileConjugateSplatOffsetUAV.SafeRelease();
		m_tileConjugateSplatOffsetSRV.SafeRelease();


		FRHICommandListBase& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		// Position buffer
		FRHIResourceCreateInfo EncodedSplatPositionCreateInfo(TEXT("EncodedSplatPositionBuffer"));
		m_encodedSplatPositionBuffer = RHICmdList.CreateBuffer(
			sizeof(EncodedSplatVector3) * m_maxSplats,
			BUF_ShaderResource | BUF_ByteAddressBuffer,
			sizeof(EncodedSplatVector3),
			ERHIAccess::SRVMask, 
			EncodedSplatPositionCreateInfo
		);

		// Colour + alpha buffer
		FRHIResourceCreateInfo EncodedSplatColourAlphaCreateInfo(TEXT("EncodedSplatColourAlphaBuffer"));
		m_encodedSplatColourAlphaBuffer = RHICmdList.CreateBuffer(
			sizeof(EncodedSplatColourAlpha) * m_maxSplats,
			BUF_ShaderResource | BUF_ByteAddressBuffer,
			sizeof(EncodedSplatColourAlpha),
			ERHIAccess::SRVMask,
			EncodedSplatColourAlphaCreateInfo
		);

		// Scale buffer
		FRHIResourceCreateInfo EncodedSplatScaleCreationInfo(TEXT("EncodedSplatScaleBuffer"));
		m_encodedSplatScaleBuffer = RHICmdList.CreateBuffer(
			sizeof(EncodedSplatScale) * m_maxSplats,
			BUF_ShaderResource | BUF_ByteAddressBuffer,
			sizeof(EncodedSplatScale),
			ERHIAccess::SRVMask,
			EncodedSplatScaleCreationInfo
		);

		// Rotation buffer
		FRHIResourceCreateInfo EncodedSplatRotationCreationInfo(TEXT("EncodedSplatRotationBuffer"));
		m_encodedSplatRotationBuffer = RHICmdList.CreateBuffer(
			sizeof(EncodedSplatRotation) * m_maxSplats,
			BUF_ShaderResource | BUF_ByteAddressBuffer,
			sizeof(EncodedSplatRotation),
			ERHIAccess::SRVMask,
			EncodedSplatRotationCreationInfo
		);

		// SH Coeffs buffer
		FRHIResourceCreateInfo EncodedSplatSHCoeffsCreationInfo(TEXT("EncodedSplatCoeffsBuffer"));
		m_encodedSplatSHCoeffsBuffer = RHICmdList.CreateBuffer(
			sizeof(EncodedSplat3DegreeSHCoeffs) * m_maxSplats,
			BUF_ShaderResource | BUF_ByteAddressBuffer,
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

		// TilesTouched buffer
		FRHIResourceCreateInfo SplatNumTilesTouchedInfo(TEXT("TilesTouchedBuffer"));
		m_splatNumTileTouched = RHICmdList.CreateBuffer(
			sizeof(uint32_t) * m_maxSplats,
			BUF_ShaderResource | BUF_UnorderedAccess | BUF_StructuredBuffer,
			sizeof(uint32_t),
			ERHIAccess::UAVMask,
			SplatNumTilesTouchedInfo
		);

		// tile x splat offset buffer
		FRHIResourceCreateInfo TileConjugateSplatOffsetInfo(TEXT("TileConjugateSplatOffsetBuffer"));
		m_tileConjugateSplatOffset = RHICmdList.CreateBuffer(
			sizeof(uint32_t) * m_maxSplats,
			BUF_ShaderResource | BUF_UnorderedAccess | BUF_StructuredBuffer,
			sizeof(uint32_t),
			ERHIAccess::UAVMask,
			TileConjugateSplatOffsetInfo
		);

		// Create UAV/SRV
		m_encodedSplatPositionSRV = __CreateSRV(m_encodedSplatPositionBuffer);
		m_encodedSplatColourAlphaSRV = __CreateSRV(m_encodedSplatColourAlphaBuffer);
		m_encodedSplatScaleSRV = __CreateSRV(m_encodedSplatScaleBuffer);
		m_encodedSplatRotationSRV = __CreateSRV(m_encodedSplatRotationBuffer);
		m_encodedSplatSHCoeffsSRV = __CreateSRV(m_encodedSplatSHCoeffsBuffer);

		m_splatViewUAV = __CreateUAV(m_splatViewBuffer, false, false);;
		m_splatViewSRV = __CreateSRV(m_splatViewBuffer);

		m_splatNumTileTouchedUAV = __CreateUAV(m_splatNumTileTouched, false, false);
		m_splatNumTileTouchedSRV = __CreateSRV(m_splatNumTileTouched);
		m_tileConjugateSplatOffsetUAV = __CreateUAV(m_tileConjugateSplatOffset, false, false);
		m_tileConjugateSplatOffsetSRV = __CreateSRV(m_tileConjugateSplatOffset);
	}
}

void FGaussianSplatTileRenderer::ReleaseSecondStageResources()
{

	// Release UAVs/SRVs
	if (m_tileConjugateSplatKeysUAV[0])
	{
		m_tileConjugateSplatKeysUAV[0].SafeRelease();
		m_tileConjugateSplatKeysUAV[0] = nullptr;
	}

	if (m_tileConjugateSplatKeysUAV[1])
	{
		m_tileConjugateSplatKeysUAV[1].SafeRelease();
		m_tileConjugateSplatKeysUAV[1] = nullptr;
	}

	if (m_tileConjugateSplatValuesUAV[0])
	{
		m_tileConjugateSplatValuesUAV[0].SafeRelease();
		m_tileConjugateSplatValuesUAV[0] = nullptr;
	}

	if (m_tileConjugateSplatValuesUAV[1])
	{
		m_tileConjugateSplatValuesUAV[1].SafeRelease();
		m_tileConjugateSplatValuesUAV[1] = nullptr;
	}

	if (m_tileConjugateSplatKeysSRV[0])
	{
		m_tileConjugateSplatKeysSRV[0].SafeRelease();
		m_tileConjugateSplatKeysSRV[0] = nullptr;
	}

	if (m_tileConjugateSplatKeysSRV[1])
	{
		m_tileConjugateSplatKeysSRV[1].SafeRelease();
		m_tileConjugateSplatKeysSRV[1] = nullptr;
	}

	if (m_tileConjugateSplatValuesSRV[0])
	{
		m_tileConjugateSplatValuesSRV[0].SafeRelease();
		m_tileConjugateSplatValuesSRV[0] = nullptr;
	}
	
	if (m_tileConjugateSplatValuesSRV[1])
	{
		m_tileConjugateSplatValuesSRV[1].SafeRelease();
		m_tileConjugateSplatValuesSRV[1] = nullptr;
	}

	if (m_tileToSplatRangesUAV)
	{
		m_tileToSplatRangesUAV.SafeRelease();
		m_tileToSplatRangesUAV = nullptr;
	}

	if (m_tileToSplatRangesSRV)
	{
		m_tileToSplatRangesSRV.SafeRelease();
		m_tileToSplatRangesSRV = nullptr;
	}

	// Release buffers
	if (m_tileConjugateSplatKeys[0])
	{
		m_tileConjugateSplatKeys[0].SafeRelease();
		m_tileConjugateSplatKeys[0] = nullptr;
	}

	if (m_tileConjugateSplatKeys[1])
	{
		m_tileConjugateSplatKeys[1].SafeRelease();
		m_tileConjugateSplatKeys[1] = nullptr;
	}

	if (m_tileConjugateSplatValues[0])
	{
		m_tileConjugateSplatValues[0].SafeRelease();
		m_tileConjugateSplatValues[0] = nullptr;
	}

	if (m_tileConjugateSplatValues[1])
	{
		m_tileConjugateSplatValues[1].SafeRelease();
		m_tileConjugateSplatValues[1] = nullptr;
	}

	if (m_tileToSplatRanges)
	{
		m_tileToSplatRanges.SafeRelease();
		m_tileToSplatRanges = nullptr;
	}

}

void FGaussianSplatTileRenderer::ReserveSecondStageResources(uint32_t tileConjugateSplatCount, const FVector4& InScreenParam)
{
	// Second stage resources are dependent on tileConjugateSplatCount, numTileX and numTileY
	uint32_t numTileX = FMath::DivideAndRoundUp<uint32>(InScreenParam.X, TILE_SIZE);
	uint32_t numTileY = FMath::DivideAndRoundUp<uint32>(InScreenParam.Y, TILE_SIZE);

	if (m_maxTileConjugateSplat < tileConjugateSplatCount || numTileX != m_numTileX || numTileY != m_numTileY)
	{
		m_maxTileConjugateSplat = FMath::RoundUpToPowerOfTwo(tileConjugateSplatCount);
		m_numTileX = numTileX;
		m_numTileY = numTileY;

		// Release buffers
		m_tileConjugateSplatKeys[0].SafeRelease();
		m_tileConjugateSplatKeys[1].SafeRelease();
		m_tileConjugateSplatValues[0].SafeRelease();
		m_tileConjugateSplatValues[1].SafeRelease();
		m_tileToSplatRanges.SafeRelease();

		// Release UAVs/SRVs
		m_tileConjugateSplatKeysUAV[0].SafeRelease();
		m_tileConjugateSplatKeysUAV[1].SafeRelease();
		m_tileConjugateSplatValuesUAV[0].SafeRelease();
		m_tileConjugateSplatValuesUAV[1].SafeRelease();
		m_tileConjugateSplatKeysSRV[0].SafeRelease();
		m_tileConjugateSplatKeysSRV[1].SafeRelease();
		m_tileConjugateSplatValuesSRV[0].SafeRelease();
		m_tileConjugateSplatValuesSRV[1].SafeRelease();
		m_tileToSplatRangesUAV.SafeRelease();
		m_tileToSplatRangesSRV.SafeRelease();

		FRHICommandListBase& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		
		// Create conjugate key/list buffers
		for (int i = 0; i < 2; ++i)
		{
			FRHIResourceCreateInfo TileConjugateSplatKeysCreateInfo(*FString::Printf(TEXT("TileConjugateSplatKeyBuffer%d"), i));
			m_tileConjugateSplatKeys[i] = RHICmdList.CreateBuffer(
				sizeof(uint32_t) * m_maxTileConjugateSplat,
				BUF_ShaderResource | BUF_UnorderedAccess | BUF_StructuredBuffer,
				sizeof(uint32_t),
				ERHIAccess::UAVMask,
				TileConjugateSplatKeysCreateInfo
			);

			FRHIResourceCreateInfo TileConjugateSplatValuesCreateInfo(*FString::Printf(TEXT("TileConjugateSplatValueBuffer%d"), i));
			m_tileConjugateSplatValues[i] = RHICmdList.CreateBuffer(
				sizeof(uint32_t) * m_maxTileConjugateSplat,
				BUF_ShaderResource | BUF_UnorderedAccess | BUF_StructuredBuffer,
				sizeof(uint32_t),
				ERHIAccess::UAVMask,
				TileConjugateSplatValuesCreateInfo
			);
		}

		// Tile-splat ranges buffer
		FRHIResourceCreateInfo TileToSplatRangesCreateInfo(TEXT("TileToSplatRanges"));
		m_tileToSplatRanges = RHICmdList.CreateBuffer(
			sizeof(uint32_t) * 2 * m_numTileX * m_numTileY,
			BUF_ShaderResource | BUF_UnorderedAccess | BUF_StructuredBuffer,
			sizeof(uint32_t) * 2,
			ERHIAccess::UAVMask,
			TileToSplatRangesCreateInfo);




		// Create UAVs
		for (int i = 0; i < 2; ++i)
		{
			m_tileConjugateSplatKeysUAV[i] = __CreateUAV(m_tileConjugateSplatKeys[i], false, false);
			m_tileConjugateSplatValuesUAV[i] = __CreateUAV(m_tileConjugateSplatValues[i], false, false);
		}
		m_tileToSplatRangesUAV = __CreateUAV(m_tileToSplatRanges, false, false);

		// Create SRVs
		for (int i = 0; i < 2; ++i)
		{
			m_tileConjugateSplatKeysSRV[i] = __CreateSRV(m_tileConjugateSplatKeys[i]);
			m_tileConjugateSplatValuesSRV[i] = __CreateSRV(m_tileConjugateSplatValues[i]);
		}

		m_tileToSplatRangesSRV = __CreateSRV(m_tileToSplatRanges);
	}
}



static void RunInclusiveSumTest(FRHICommandListImmediate& RHICmdList, int32 TestSize, ERHIFeatureLevel::Type FeatureLevel)
{
	FRandomStream RandomStream(FPlatformTime::Cycles());

	TArray<uint32> Values;
	TArray<uint32> RefInclusiveSum;
	TArray<uint32> GPUInclusiveSum;

	Values.Reserve(TestSize);
	Values.AddUninitialized(TestSize);
	RefInclusiveSum.Reserve(TestSize);
	RefInclusiveSum.AddUninitialized(TestSize);
	GPUInclusiveSum.Reserve(TestSize);
	GPUInclusiveSum.AddUninitialized(TestSize);


	for (int32 i = 0; i < TestSize; ++i)
	{
		Values[i] = RandomStream.GetUnsignedInt() % 2394; // Should not exceeds max(numTileTouched) = (tileX * tileY)
	}

	// Perform a reference inclusive sum on the CPU.
	for (int32 i = 0; i < TestSize; ++i)
	{
		RefInclusiveSum[i] = Values[i] + ((i == 0) ? 0 : RefInclusiveSum[i - 1]);
	}

	// Allocate GPU resources
	FRHIResourceCreateInfo InputBufferCreateInfo(TEXT("InputBuffer"));
	FBufferRHIRef InputBuffer  = RHICmdList.CreateBuffer(
		sizeof(uint32_t) * TestSize,
		BUF_ShaderResource | BUF_UnorderedAccess | BUF_StructuredBuffer,
		sizeof(uint32_t),
		ERHIAccess::UAVMask,
		InputBufferCreateInfo
	);

	FUnorderedAccessViewRHIRef InputBufferUAV = __CreateUAV(InputBuffer, false, false);
	FShaderResourceViewRHIRef InputBufferSRV = __CreateSRV(InputBuffer);

	FRHIResourceCreateInfo IntermediateInclusiveSumGroupSumsBufferInfo(TEXT("GroupSumsBuffer"));
	FBufferRHIRef GroupSumBuffer = RHICmdList.CreateBuffer(
		sizeof(uint32_t) * INCLUSIVE_SUM_BLOCK_SIZE,
		BUF_ShaderResource | BUF_UnorderedAccess | BUF_StructuredBuffer,
		sizeof(uint32_t),
		ERHIAccess::UAVMask,
		IntermediateInclusiveSumGroupSumsBufferInfo
	);

	FUnorderedAccessViewRHIRef GroupSumBufferUAV = __CreateUAV(GroupSumBuffer, false, false);
	FShaderResourceViewRHIRef GroupSumSRV = __CreateSRV(GroupSumBuffer);

	FRHIResourceCreateInfo IntermediateInclusiveSumGroupSumsScanBufferInfo(TEXT("GroupSumsScanBuffer"));
	FBufferRHIRef GroupSumScanBuffer = RHICmdList.CreateBuffer(
		sizeof(uint32_t) * INCLUSIVE_SUM_GROUP_SIZE,
		BUF_ShaderResource | BUF_UnorderedAccess | BUF_StructuredBuffer,
		sizeof(uint32_t),
		ERHIAccess::UAVMask,
		IntermediateInclusiveSumGroupSumsScanBufferInfo
	);

	FUnorderedAccessViewRHIRef GroupSumScanBufferUAV = __CreateUAV(GroupSumScanBuffer, false, false);
	FShaderResourceViewRHIRef GroupSumScanSRV = __CreateSRV(GroupSumScanBuffer);
	

	FRHIResourceCreateInfo OutputBufferCreateInfo(TEXT("OutputBuffer"));
	FBufferRHIRef OutputBuffer = RHICmdList.CreateBuffer(
		sizeof(uint32_t) * TestSize,
		BUF_ShaderResource | BUF_UnorderedAccess | BUF_StructuredBuffer,
		sizeof(uint32_t),
		ERHIAccess::UAVMask,
		OutputBufferCreateInfo
	);

	FUnorderedAccessViewRHIRef OutputBufferUAV = __CreateUAV(OutputBuffer, false, false);

	// Upload buffer to GPU
	uint32_t* pInputBuffer = (uint32_t*)RHICmdList.LockBuffer(InputBuffer, 0, sizeof(uint32_t) * TestSize, RLM_WriteOnly);
	FMemory::Memcpy(pInputBuffer, Values.GetData(), sizeof(uint32_t) * TestSize);
	RHICmdList.UnlockBuffer(InputBuffer);

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	uint32 ThreadGroupCount;
	// Perform 3 stage GPU inclusive sum
	// 
	{
		// Inclusive sum -> _TileConjugatedSplatOffsetBuffer
		TShaderMapRef<FInclusiveSumComputeShader> CSInclusiveSum(ShaderMap);
		SetComputePipelineState(RHICmdList, CSInclusiveSum.GetComputeShader());

		//RHICmdList.Transition(FRHITransitionInfo(InputBufferUAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute));
		RHICmdList.Transition(FRHITransitionInfo(InputBuffer, ERHIAccess::Unknown, ERHIAccess::SRVCompute));
		RHICmdList.Transition(FRHITransitionInfo(OutputBufferUAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute));
		RHICmdList.Transition(FRHITransitionInfo(GroupSumBufferUAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute));

		// Bind
		FInclusiveSumComputeShader::FParameters ShaderParameters;
		ShaderParameters.input = InputBufferSRV;
		ShaderParameters.output = OutputBufferUAV;
		ShaderParameters.groupSumsOutput = GroupSumBufferUAV;
		ShaderParameters.io_size = TestSize;
		SetShaderParameters(RHICmdList, CSInclusiveSum, CSInclusiveSum.GetComputeShader(), ShaderParameters);

		// Dispatch 1: Inclusive Sum
		ThreadGroupCount = FMath::DivideAndRoundUp<uint32>(TestSize, INCLUSIVE_SUM_BLOCK_SIZE);
		check(ThreadGroupCount <= INCLUSIVE_SUM_GROUP_SIZE) // group count cannot exceed INCLUSIVE_SUM_GROUP_SIZE otherwise the inclusive sum won't be correct
			RHICmdList.DispatchComputeShader(ThreadGroupCount, 1, 1);

		UnsetShaderUAVs(RHICmdList, CSInclusiveSum, CSInclusiveSum.GetComputeShader());
	}


	// Dispatch 2: Inclusive Group Sum
	{
		TShaderMapRef<FInclusiveGroupSumComputeShader> CSInclusiveGroupSum(ShaderMap);
		SetComputePipelineState(RHICmdList, CSInclusiveGroupSum.GetComputeShader());

		//RHICmdList.Transition(FRHITransitionInfo(GroupSumBufferUAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute));
		RHICmdList.Transition(FRHITransitionInfo(GroupSumBuffer, ERHIAccess::UAVCompute, ERHIAccess::SRVCompute));
		RHICmdList.Transition(FRHITransitionInfo(GroupSumScanBufferUAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute));

		uint32_t groupCount = FMath::DivideAndRoundUp<uint32_t>(TestSize, INCLUSIVE_SUM_BLOCK_SIZE);
		check(groupCount <= INCLUSIVE_SUM_GROUP_SIZE);

		FInclusiveGroupSumComputeShader::FParameters ShaderParameters;
		ShaderParameters.groupSumsInput = GroupSumSRV;
		ShaderParameters.groupSumsScanOutput = GroupSumScanBufferUAV;
		ShaderParameters.numGroups = groupCount;

		// Bind
		SetShaderParameters(RHICmdList, CSInclusiveGroupSum, CSInclusiveGroupSum.GetComputeShader(), ShaderParameters);

		ThreadGroupCount = 1; // Only one group, unless splatCount > (INCLUSIVE_SUM_BLOCK_SIZE*INCLUSIVE_SUM_GROUP_SIZE) which is asserted before
		RHICmdList.DispatchComputeShader(ThreadGroupCount, 1, 1);

		// Unbind
		UnsetShaderUAVs(RHICmdList, CSInclusiveGroupSum, CSInclusiveGroupSum.GetComputeShader());
	}

	// Dispatch 3: Add group offset
	{
		TShaderMapRef<FInclusiveSumAddGroupOffsetComputeShader> CSAddGroupOffset(ShaderMap);
		SetComputePipelineState(RHICmdList, CSAddGroupOffset.GetComputeShader());

		//RHICmdList.Transition(FRHITransitionInfo(GroupSumScanBufferUAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute));
		RHICmdList.Transition(FRHITransitionInfo(GroupSumScanBuffer, ERHIAccess::UAVCompute, ERHIAccess::SRVCompute));
		RHICmdList.Transition(FRHITransitionInfo(OutputBufferUAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute));

		// Bind
		FInclusiveSumAddGroupOffsetComputeShader::FParameters ShaderParameters;
		ShaderParameters.groupSumsScanInput = GroupSumScanSRV;
		ShaderParameters.finalOutput = OutputBufferUAV;
		ShaderParameters.io_size = TestSize;
		SetShaderParameters(RHICmdList, CSAddGroupOffset, CSAddGroupOffset.GetComputeShader(), ShaderParameters);
		ThreadGroupCount = FMath::DivideAndRoundUp<uint32_t>(TestSize, INCLUSIVE_SUM_BLOCK_SIZE);
		RHICmdList.DispatchComputeShader(ThreadGroupCount, 1, 1);
		// Unbind
		UnsetShaderUAVs(RHICmdList, CSAddGroupOffset, CSAddGroupOffset.GetComputeShader());
	}


	// Download result from GPU
	RHICmdList.Transition(FRHITransitionInfo(OutputBufferUAV, ERHIAccess::Unknown, ERHIAccess::ReadOnlyExclusiveMask));
	uint32_t* pLocked = (uint32_t*)RHICmdList.LockBuffer(OutputBuffer, 0, sizeof(uint32_t) * TestSize, RLM_ReadOnly);
	FMemory::Memcpy(GPUInclusiveSum.GetData(), pLocked, sizeof(uint32_t) * TestSize);
	RHICmdList.UnlockBuffer(OutputBuffer);

	// Deallocation
	InputBufferUAV.SafeRelease();
	OutputBufferUAV.SafeRelease();
	GroupSumBufferUAV.SafeRelease();
	GroupSumScanBufferUAV.SafeRelease();

	InputBufferSRV.SafeRelease();
	GroupSumSRV.SafeRelease();
	GroupSumScanSRV.SafeRelease();

	// Compare
	bool bSucceeded = true;
	for (int i = 0; i < TestSize; ++i)
	{
		if (GPUInclusiveSum[i] != RefInclusiveSum[i])
		{
			bSucceeded = false;
			break;
		}
	}

	if (bSucceeded)
	{
		UE_LOG(EvercoastVoxelDecoderLog, Log, TEXT("InclusiveSum test (%d values) succeeded."), TestSize);
	}
	else
	{
		UE_LOG(EvercoastVoxelDecoderLog, Log, TEXT("InclusiveSum test (%d values) FAILED."), TestSize);
	}
}

void FGaussianSplatTileRenderer::DEBUG_TestInclusiveSum(FRHICommandListImmediate& RHICmdList, int32_t TestSize)
{
	RunInclusiveSumTest(RHICmdList, TestSize, GMaxRHIFeatureLevel);
}

uint32_t FGaussianSplatTileRenderer::RunFirstStagePipeline_RenderThread(FRHICommandListImmediate& RHICmdList, const FMatrix& InObjectToWorld, const FMatrix& InView, const FMatrix& InProj, const FVector& InCameraPositionWS,
	uint32_t numSplats, const FVector4& InScreenParam, float InCov2DSqrtKernelSize, bool InShowSH0, bool InShowSH1, bool InShowSH2, bool InShowSH3)
{


	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	uint32 ThreadGroupCount;


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
	RHICmdList.Transition(FRHITransitionInfo(m_splatNumTileTouched, ERHIAccess::Unknown, ERHIAccess::UAVMask));

	TShaderMapRef<FGaussianSplatTileRendererPreprocessComputeShader> CalcSplatViewDataCS(ShaderMap);


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
		InView, InProj, InCameraPositionWS, InScreenParam, InCov2DSqrtKernelSize,
		InShowSH0, InShowSH1, InShowSH2, InShowSH3
	);
	CalcSplatViewDataCS->SetupIOBuffers(BatchedShaderParams2, m_encodedSplatPositionSRV, m_encodedSplatColourAlphaSRV, m_encodedSplatScaleSRV, m_encodedSplatRotationSRV, m_encodedSplatSHCoeffsSRV, m_splatViewUAV, m_splatNumTileTouchedUAV);
	RHICmdList.SetBatchedShaderParameters(CalcSplatViewDataCS.GetComputeShader(), BatchedShaderParams2);

#else
		// Transform data only can get from SceneProxy::GetDynamicMeshElements, so this function is called with up-to-date view data
	CalcSplatViewDataCS->SetupTransformsAndUniforms(RHICmdList, InObjectToWorld, WorldToObject,
		numSplats,
		m_encodedGaussianSplats->shDegree,
		shDim,
		m_encodedGaussianSplats->positionScalar,
		InView, InProj, InCameraPositionWS, InScreenParam, InCov2DSqrtKernelSize,
		InShowSH0, InShowSH1, InShowSH2, InShowSH3
	);
	CalcSplatViewDataCS->SetupIOBuffers(RHICmdList, m_encodedSplatPositionSRV, m_encodedSplatColourAlphaSRV, m_encodedSplatScaleSRV, m_encodedSplatRotationSRV, m_encodedSplatSHCoeffsSRV, m_splatViewUAV, m_splatNumTileTouchedUAV);
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

	uint32_t numTileConjugateSplat = 0;
	uint32_t inclusiveSumGroupCount = FMath::DivideAndRoundUp<uint32_t>(numSplats, INCLUSIVE_SUM_BLOCK_SIZE);
	if (inclusiveSumGroupCount > INCLUSIVE_SUM_GROUP_SIZE)
	{
		// For now we will have to use CPU inclusive sum for large splat number, like .ply scenary file

		// Extremely simple on CPU but requires locking two buffers and doing a sequential calculation
		// Let's see how bad it will be when numSplats is large...
		uint32_t* pNumTileTouched = (uint32_t*)RHICmdList.LockBuffer(m_splatNumTileTouched, 0, sizeof(uint32_t) * numSplats, RLM_ReadOnly);
		uint32_t* pTileConjugateSplatOffset = (uint32_t*)RHICmdList.LockBuffer(m_tileConjugateSplatOffset, 0, sizeof(uint32_t) * numSplats, RLM_WriteOnly);

		// Perform a reference inclusive sum on the CPU.
		for (uint32_t i = 0; i < numSplats; ++i)
		{
			pTileConjugateSplatOffset[i] = pNumTileTouched[i] + ((i == 0) ? 0 : pTileConjugateSplatOffset[i - 1]);
		}

		// Output numTileConjugateSplat
		numTileConjugateSplat = pTileConjugateSplatOffset[numSplats - 1];

		RHICmdList.UnlockBuffer(m_splatNumTileTouched);
		RHICmdList.UnlockBuffer(m_tileConjugateSplatOffset);

		RHICmdList.Transition(FRHITransitionInfo(m_tileConjugateSplatOffset, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	}
	else
	{
		// Algorithm outline:
		// This is where tile based volume renderer differs from the alpha blending quad renderer.
		// 1. Perform inclusive sum of tiles_touched, this will give point_offset(_TileConjugateSplatOffsetBuffer), mapping splat index to the index of tile conjugated splat array. Size=numSplats
		// 2. Read out the tile-splat conjugated render count num_rendered(_NumTileConjugateSplat)
		// This will be end of the first stage.
		// After that, on CPU:
		// 3. Allocate 4 sorting buffers based on num_rendered, if needed
		// After that,  on GPU
		// 4. Perform CSDuplicateWithKeys, filling unsorted buffers
		// 5. Perform the radix sorting. We will get sorted buffers with key= (tile_id|depth), value=splat_idx
		// 6. Perform CSIdentifyTileRanges, gives us _TileToSplatRanges so we know for each tile how many and what splats will be drawn on
		// 7. Perform CSRenderViewData/CSRenderViewDataNoGroupSharedMemory, 1 tile = 1 Group, 1 pixel with the tile = 1 thread. This will output the final image(phew)


		// Inclusive Sum itself will contain 3 patches:
		// 1. Inclusive sum scan over all TilesTouched, get a per-group summed partial result, and an auxiliary groupSums result
		// 2. Inclusive sum scan over all groupSums, get a summed result for groups: groupSumsScan
		// 3. Add groupSumsScan to output


		// Inclusive sum -> _TileConjugatedSplatOffsetBuffer
		{
			TShaderMapRef<FInclusiveSumComputeShader> CSInclusiveSum(ShaderMap);
			SetComputePipelineState(RHICmdList, CSInclusiveSum.GetComputeShader());

			//RHICmdList.Transition(FRHITransitionInfo(splatNumTileTouchedSRV, ERHIAccess::Unknown, ERHIAccess::SRVCompute));
			RHICmdList.Transition(FRHITransitionInfo(m_splatNumTileTouched, ERHIAccess::UAVMask, ERHIAccess::SRVCompute));
			RHICmdList.Transition(FRHITransitionInfo(m_tileConjugateSplatOffset, ERHIAccess::Unknown, ERHIAccess::UAVCompute));
			RHICmdList.Transition(FRHITransitionInfo(m_intermInclusiveSumGroupSumsBuffer, ERHIAccess::Unknown, ERHIAccess::UAVCompute));

			// Bind
			FInclusiveSumComputeShader::FParameters ShaderParameters;
			ShaderParameters.input = m_splatNumTileTouchedSRV;
			ShaderParameters.output = m_tileConjugateSplatOffsetUAV;
			ShaderParameters.groupSumsOutput = m_intermInclusiveSumGroupSumsUAV;
			ShaderParameters.io_size = numSplats;
			SetShaderParameters(RHICmdList, CSInclusiveSum, CSInclusiveSum.GetComputeShader(), ShaderParameters);


			// Dispatch 1: Inclusive Sum
			ThreadGroupCount = FMath::DivideAndRoundUp<uint32>(numSplats, INCLUSIVE_SUM_BLOCK_SIZE);
			check(ThreadGroupCount <= INCLUSIVE_SUM_GROUP_SIZE) // group count cannot exceed INCLUSIVE_SUM_GROUP_SIZE otherwise the inclusive sum won't be correct
				RHICmdList.DispatchComputeShader(ThreadGroupCount, 1, 1);

			// Unbind
			UnsetShaderUAVs(RHICmdList, CSInclusiveSum, CSInclusiveSum.GetComputeShader());
			UnsetShaderSRVs(RHICmdList, CSInclusiveSum, CSInclusiveSum.GetComputeShader());
		}

		{
			// Dispatch 2: Inclusive Group Sum
			TShaderMapRef<FInclusiveGroupSumComputeShader> CSInclusiveGroupSum(ShaderMap);
			SetComputePipelineState(RHICmdList, CSInclusiveGroupSum.GetComputeShader());

			RHICmdList.Transition(FRHITransitionInfo(m_intermInclusiveSumGroupSumsBuffer, ERHIAccess::UAVCompute, ERHIAccess::SRVCompute));
			RHICmdList.Transition(FRHITransitionInfo(m_intermInclusiveSumGroupSumsScanBuffer, ERHIAccess::Unknown, ERHIAccess::UAVCompute));

			uint32_t groupCount = FMath::DivideAndRoundUp<uint32_t>(numSplats, INCLUSIVE_SUM_BLOCK_SIZE);
			check(groupCount <= INCLUSIVE_SUM_GROUP_SIZE);

			// Bind
			FInclusiveGroupSumComputeShader::FParameters ShaderParameters;
			ShaderParameters.groupSumsInput = m_intermInclusiveSumGroupSumsSRV;
			ShaderParameters.groupSumsScanOutput = m_intermInclusiveSumGroupSumsScanUAV;
			ShaderParameters.numGroups = groupCount;
			SetShaderParameters(RHICmdList, CSInclusiveGroupSum, CSInclusiveGroupSum.GetComputeShader(), ShaderParameters);

			ThreadGroupCount = 1; // Only one group, unless numSplats > (INCLUSIVE_SUM_BLOCK_SIZE*INCLUSIVE_SUM_GROUP_SIZE) which is asserted before
			RHICmdList.DispatchComputeShader(ThreadGroupCount, 1, 1);

			UnsetShaderUAVs(RHICmdList, CSInclusiveGroupSum, CSInclusiveGroupSum.GetComputeShader());
			UnsetShaderSRVs(RHICmdList, CSInclusiveGroupSum, CSInclusiveGroupSum.GetComputeShader());
		}


		{
			// Dispatch 3: Add group offset
			TShaderMapRef<FInclusiveSumAddGroupOffsetComputeShader> CSAddGroupOffset(ShaderMap);
			SetComputePipelineState(RHICmdList, CSAddGroupOffset.GetComputeShader());

			RHICmdList.Transition(FRHITransitionInfo(m_intermInclusiveSumGroupSumsScanBuffer, ERHIAccess::UAVCompute, ERHIAccess::SRVCompute));
			RHICmdList.Transition(FRHITransitionInfo(m_tileConjugateSplatOffset, ERHIAccess::Unknown, ERHIAccess::UAVCompute));


			// Bind
			FInclusiveSumAddGroupOffsetComputeShader::FParameters ShaderParameters;
			ShaderParameters.groupSumsScanInput = m_intermInclusiveSumGroupSumsScanSRV;
			ShaderParameters.finalOutput = m_tileConjugateSplatOffsetUAV;
			ShaderParameters.io_size = numSplats;
			SetShaderParameters(RHICmdList, CSAddGroupOffset, CSAddGroupOffset.GetComputeShader(), ShaderParameters);
			ThreadGroupCount = FMath::DivideAndRoundUp<uint32_t>(numSplats, INCLUSIVE_SUM_BLOCK_SIZE);
			RHICmdList.DispatchComputeShader(ThreadGroupCount, 1, 1);

			// Unbind
			UnsetShaderUAVs(RHICmdList, CSAddGroupOffset, CSAddGroupOffset.GetComputeShader());
			UnsetShaderSRVs(RHICmdList, CSAddGroupOffset, CSAddGroupOffset.GetComputeShader());
		}


		// PEEK IS NECESSARY
		RHICmdList.Transition(FRHITransitionInfo(m_tileConjugateSplatOffset, ERHIAccess::UAVCompute, ERHIAccess::ReadOnlyExclusiveMask));
		uint32_t* pLockedLastElement = (uint32_t*)RHICmdList.LockBuffer(m_tileConjugateSplatOffset, sizeof(uint32_t) * (numSplats - 1), sizeof(uint32_t), RLM_ReadOnly);
		numTileConjugateSplat = pLockedLastElement[0];
		RHICmdList.UnlockBuffer(m_tileConjugateSplatOffset);

		RHICmdList.Transition(FRHITransitionInfo(m_tileConjugateSplatOffset, ERHIAccess::ReadOnlyExclusiveMask, ERHIAccess::SRVMask));
	}

	return numTileConjugateSplat;
}

bool FGaussianSplatTileRenderer::RunSecondStagePipeline_RenderThread(FRHICommandListImmediate& RHICmdList, uint32_t numSplats, uint32_t numTileConjugateSplat, const FVector4& InScreenParam, const FVector4& DepthOutputThreshold)
{


	if (numSplats == 0 || numTileConjugateSplat == 0)
		return false;

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	uint32_t ThreadGroupCount;

	{
		// Pass 1
		// Perform CSDuplicateWithKeys, filling unsorted buffers
		TShaderMapRef<FDuplicateGaussianSplatWithKeysCS> DuplicateGaussianSplatWithKeysCS(ShaderMap);
		SetComputePipelineState(RHICmdList, DuplicateGaussianSplatWithKeysCS.GetComputeShader());

		RHICmdList.Transition(FRHITransitionInfo(m_splatViewBuffer, ERHIAccess::UAVMask, ERHIAccess::SRVCompute));
		RHICmdList.Transition(FRHITransitionInfo(m_tileConjugateSplatOffset, ERHIAccess::SRVMask, ERHIAccess::SRVCompute));
		RHICmdList.Transition(FRHITransitionInfo(m_tileConjugateSplatKeys[0], ERHIAccess::Unknown, ERHIAccess::UAVCompute));
		RHICmdList.Transition(FRHITransitionInfo(m_tileConjugateSplatValues[0], ERHIAccess::Unknown, ERHIAccess::UAVCompute));

		// Setup shader parameters
		FDuplicateGaussianSplatWithKeysCS::FParameters ShaderParameters;
		ShaderParameters.SplatCount = numSplats;
		ShaderParameters.VecScreenParams = FVector4f(InScreenParam);
		ShaderParameters.TheSplatViewData = m_splatViewSRV;
		ShaderParameters.TileConjugateSplatOffsetBuffer = m_tileConjugateSplatOffsetSRV;
		ShaderParameters.TileConjugateSplatKeys = m_tileConjugateSplatKeysUAV[0];
		ShaderParameters.TileConjugateSplatValues = m_tileConjugateSplatValuesUAV[0];

		// Bind
		SetShaderParameters(RHICmdList, DuplicateGaussianSplatWithKeysCS, DuplicateGaussianSplatWithKeysCS.GetComputeShader(), ShaderParameters);

		// Dispatch
		ThreadGroupCount = FMath::DivideAndRoundUp<uint32_t>(numSplats, DUPLICATE_KEY_THREADS);
		RHICmdList.DispatchComputeShader(ThreadGroupCount, 1, 1);

		// Unbind
		UnsetShaderUAVs(RHICmdList, DuplicateGaussianSplatWithKeysCS, DuplicateGaussianSplatWithKeysCS.GetComputeShader());
		UnsetShaderSRVs(RHICmdList, DuplicateGaussianSplatWithKeysCS, DuplicateGaussianSplatWithKeysCS.GetComputeShader());
	}


	FUnorderedAccessViewRHIRef sortedKeyListUAV, sortedValueListUAV;
	FShaderResourceViewRHIRef sortedKeyListSRV, sortedValueListSRV;
	FBufferRHIRef sortedKeyList, sortedValueList;
	{
		// Pass 2
		// GPU Radix sort, element count is different
		FGPUSortBuffers SortBuffers; // fill in buffers
		SortBuffers.RemoteKeySRVs[0] = m_tileConjugateSplatKeysSRV[0];
		SortBuffers.RemoteKeySRVs[1] = m_tileConjugateSplatKeysSRV[1];
		SortBuffers.RemoteValueSRVs[0] = m_tileConjugateSplatValuesSRV[0];
		SortBuffers.RemoteValueSRVs[1] = m_tileConjugateSplatValuesSRV[1];

		SortBuffers.RemoteKeyUAVs[0] = m_tileConjugateSplatKeysUAV[0];
		SortBuffers.RemoteKeyUAVs[1] = m_tileConjugateSplatKeysUAV[1];
		SortBuffers.RemoteValueUAVs[0] = m_tileConjugateSplatValuesUAV[0];
		SortBuffers.RemoteValueUAVs[1] = m_tileConjugateSplatValuesUAV[1];


		// Run gpu sorter
		uint32_t resultBufferIndex = SortGPUBuffers(RHICmdList, SortBuffers, 0, 0xFFFFFFFF, (int32)numTileConjugateSplat, GMaxRHIFeatureLevel);

		// Identify splat ranges among the big conjugate list
		sortedKeyListUAV = m_tileConjugateSplatKeysUAV[resultBufferIndex];
		sortedValueListUAV = m_tileConjugateSplatValuesUAV[resultBufferIndex];

		sortedKeyListSRV = m_tileConjugateSplatKeysSRV[resultBufferIndex];
		sortedValueListSRV = m_tileConjugateSplatValuesSRV[resultBufferIndex];
		sortedKeyList = m_tileConjugateSplatKeys[resultBufferIndex];
		sortedValueList = m_tileConjugateSplatValues[resultBufferIndex];
	}


	{
		// Pass 3
		// Identify splat ranges
		//RHICmdList.Transition(FRHITransitionInfo(sortedKeyListUAV, ERHIAccess::Unknown, ERHIAccess::UAVMask));
		RHICmdList.Transition(FRHITransitionInfo(sortedKeyList, ERHIAccess::Unknown, ERHIAccess::SRVCompute));
		RHICmdList.Transition(FRHITransitionInfo(m_tileToSplatRangesUAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute));

		TShaderMapRef<FIdentifyGaussianSplatRangesCS> IdentifyGaussianSplatRangesCS(ShaderMap);
		SetComputePipelineState(RHICmdList, IdentifyGaussianSplatRangesCS.GetComputeShader());

		FIdentifyGaussianSplatRangesCS::FParameters ShaderParameters;
		ShaderParameters.NumTileConjugateSplat = numTileConjugateSplat;
		ShaderParameters.TileConjugateSplatKeysSorted = sortedKeyListSRV;
		ShaderParameters.TileToSplatRanges = m_tileToSplatRangesUAV;


		// Bind
		SetShaderParameters(RHICmdList, IdentifyGaussianSplatRangesCS, IdentifyGaussianSplatRangesCS.GetComputeShader(), ShaderParameters);

		// Dispatch
		ThreadGroupCount = FMath::DivideAndRoundUp<uint32_t>(numTileConjugateSplat, FIND_RANGE_THREADS);
		RHICmdList.DispatchComputeShader(ThreadGroupCount, 1, 1);

		// Unbind
		UnsetShaderUAVs(RHICmdList, IdentifyGaussianSplatRangesCS, IdentifyGaussianSplatRangesCS.GetComputeShader());
		UnsetShaderSRVs(RHICmdList, IdentifyGaussianSplatRangesCS, IdentifyGaussianSplatRangesCS.GetComputeShader());
	}

	// Render View
	// check if the render target resource UAV is ready
	if (m_outputColourRenderTargetUAV && m_outputDepthRenderTargetUAV)
	{
		// Pass 4
		TShaderMapRef<FGaussianSplatTileRendererCS> GaussianSplatTileRendererCS(ShaderMap);
		SetComputePipelineState(RHICmdList, GaussianSplatTileRendererCS.GetComputeShader());

		// Setup shader parameters
		FUintVector2 textureSize(1, 1);

		if (m_outputColourRenderTarget)
		{
			textureSize = FUintVector2(m_outputColourRenderTarget->GetSizeX(), m_outputColourRenderTarget->GetSizeY());
		}

		FUintVector2 depthTextureSize(1, 1);
		if (m_outputDepthRenderTarget)
		{
			depthTextureSize = FUintVector2(m_outputDepthRenderTarget->GetSizeX(), m_outputDepthRenderTarget->GetSizeY());
		}

		// render texture to UAV compute
		RHICmdList.Transition(FRHITransitionInfo(m_splatViewBuffer, ERHIAccess::Unknown, ERHIAccess::SRVCompute));
		RHICmdList.Transition(FRHITransitionInfo(m_tileToSplatRanges, ERHIAccess::Unknown, ERHIAccess::SRVCompute));
		RHICmdList.Transition(FRHITransitionInfo(sortedValueList, ERHIAccess::Unknown, ERHIAccess::SRVCompute));
		RHICmdList.Transition(FRHITransitionInfo(m_outputColourRenderTarget, ERHIAccess::Unknown, ERHIAccess::UAVCompute));
		RHICmdList.Transition(FRHITransitionInfo(m_outputDepthRenderTarget, ERHIAccess::Unknown, ERHIAccess::UAVCompute));

		FGaussianSplatTileRendererCS::FParameters ShaderParameters;
		ShaderParameters.SplatCount = numSplats;
		ShaderParameters.NumTileConjugateSplat = numTileConjugateSplat;
		ShaderParameters.VecScreenParams = FVector4f(InScreenParam);
		ShaderParameters.OutputTextureSize = textureSize;
		ShaderParameters.DepthOutputThreshold = FVector4f(DepthOutputThreshold);

		ShaderParameters.TheSplatViewData = m_splatViewSRV;
		ShaderParameters.TileConjugateSplatValuesSorted = sortedValueListSRV;
		ShaderParameters.TileToSplatRanges = m_tileToSplatRangesSRV;
		ShaderParameters.OutputColourTexture = m_outputColourRenderTargetUAV;
		ShaderParameters.OutputDepthTexture = m_outputDepthRenderTargetUAV;

		// Bind
		SetShaderParameters(RHICmdList, GaussianSplatTileRendererCS, GaussianSplatTileRendererCS.GetComputeShader(), ShaderParameters);

		// Dispatch
		uint32_t TileCountX = FMath::DivideAndRoundUp<uint32_t>(InScreenParam.X, GaussianSplatTileRendererCS->GetTileSizeX());
		uint32_t TileCountY = FMath::DivideAndRoundUp<uint32_t>(InScreenParam.Y, GaussianSplatTileRendererCS->GetTileSizeY());

		uint32_t ThreadGroupCountX = TileCountX;
		uint32_t ThreadGroupCountY = TileCountY;
		RHICmdList.DispatchComputeShader(ThreadGroupCountX, ThreadGroupCountY, 1);

		// Unbind
		UnsetShaderUAVs(RHICmdList, GaussianSplatTileRendererCS, GaussianSplatTileRendererCS.GetComputeShader());
		UnsetShaderSRVs(RHICmdList, GaussianSplatTileRendererCS, GaussianSplatTileRendererCS.GetComputeShader());

		// render texture To SRV
		RHICmdList.Transition(FRHITransitionInfo(m_outputColourRenderTarget, ERHIAccess::UAVCompute, ERHIAccess::SRVMask));
		RHICmdList.Transition(FRHITransitionInfo(m_outputDepthRenderTarget, ERHIAccess::UAVCompute, ERHIAccess::SRVMask));

		m_outputFrameCounter = m_savedFrameCounter;
	}

	return true;
}

bool FGaussianSplatTileRenderer::RunPipeline_RenderThread(FRHICommandListImmediate& RHICmdList, const FMatrix& InObjectToWorld, const FMatrix& InView, const FMatrix& InProj, const FVector& InCameraPositionWS, const FVector4& InScreenParam, float InCov2DSqrtKernelSize,
	bool showSH0, bool showSH1, bool showSH2, bool showSH3, const FVector4& InDepthOutputThreshold, std::shared_ptr<const EvercoastGaussianSplatCSResult> InGaussianData)
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	const uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();

	m_encodedGaussianSplats = InGaussianData;
	uint32 numSplats = InGaussianData->pointCount;
	FVector4 currScreenParam = InScreenParam;

	// Break into A/B two big chunks:
	// First stage(A Chunk):
	// 1. Init sorting buffer
	// 2. Sending CPU data to CSCalcViewData, expect it be decoded, transformed, prepared into SplatViewData AOS, along with Tiles_Touched
	// 3. Calculate the inclusive sum of Tiles_Touched array, stored into tileConjugateSplatOffsetBuffer
	// 4. Readout the last element of tileConjugateSplatOffsetBuffer. After that, stall the GPU queue

	// After stalling GPU, used the numTileConjugateSplat to initialize 4 intermediate buffers, if necessary, then restart GPU queue

	// Second stage(B Chunk):
	// 1. CSDuplicateWithKeys
	// 2. Use GPU Radix Sort to sort the combined keys
	// 3. CSIdentifyTileRanges, get TileConjugateSplatRanges
	// 4. CSRenderViewData, get transmittance and color map output

	ReservePermanentResources();
	ReserveViewDependentResources(currScreenParam);

	ReserveFirstStageResources(numSplats);
	uint32_t numTileConjugateSplat = RunFirstStagePipeline_RenderThread(RHICmdList, InObjectToWorld, InView, InProj, InCameraPositionWS, numSplats, InScreenParam, InCov2DSqrtKernelSize, showSH0, showSH1, showSH2, showSH3);

	ReserveSecondStageResources(numTileConjugateSplat, currScreenParam);
	return RunSecondStagePipeline_RenderThread(RHICmdList, numSplats, numTileConjugateSplat, currScreenParam, InDepthOutputThreshold);
}

void FGaussianSplatTileRenderer::SaveInput(const FMatrix& InObjectToWorld, const FMatrix& InView, const FMatrix& InProj, const FVector& InCameraPositionWS,
	const FVector4& InScreenParam, float InCov2DSqrtKernelSize,
	bool InShowSH0, bool InShowSH1, bool InShowSH2, bool InShowSH3,
	const FVector4& InDepthOutputThreshold,
	std::shared_ptr<const EvercoastGaussianSplatCSResult> InGaussianData)
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	m_savedFrameCounter++;

	const uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();

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
	SavedCov2DSqrtKernelSize = InCov2DSqrtKernelSize;
	SavedShowSH0 = InShowSH0;
	SavedShowSH1 = InShowSH1;
	SavedShowSH2 = InShowSH2;
	SavedShowSH3 = InShowSH3;
	SavedDepthOutputThreshold = InDepthOutputThreshold;

}

bool FGaussianSplatTileRenderer::RunPipelineWithLastSavedInput_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	return RunPipeline_RenderThread(RHICmdList, SavedObjectToWorld, SavedView, SavedProj, SavedCameraPositionWS, SavedScreenParam, SavedCov2DSqrtKernelSize, SavedShowSH0, SavedShowSH1, SavedShowSH2, SavedShowSH3, SavedDepthOutputThreshold, SavedEncodedGaussianSplats);
}

FTextureRHIRef FGaussianSplatTileRenderer::GetOutputColourRenderTarget() const
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	return m_outputColourRenderTarget;
}

FTextureRHIRef FGaussianSplatTileRenderer::GetOutputDepthRenderTarget() const
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	return m_outputDepthRenderTarget;
}

int FGaussianSplatTileRenderer::GetOutputFrameCounter() const
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	return m_outputFrameCounter;
}

int FGaussianSplatTileRenderer::GetSavedFrameCounter() const
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	return m_savedFrameCounter;
}

FVector2f FGaussianSplatTileRenderer::GetSavedOutputRenderTargetUVScale() const
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	return SavedOutputRenderTargetUVScale;
}

#endif