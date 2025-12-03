#pragma once



#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "UnrealEngineCompatibility.h"
#include "Gaussian/GaussianSplatComputeShaderConstants.h"
#include <memory>
#include <vector>
#include <mutex>


class EvercoastGaussianSplatCSResult;
class FGaussianSplatBaseQuadRenderer
{
public:
	FGaussianSplatBaseQuadRenderer();
	virtual ~FGaussianSplatBaseQuadRenderer();

	FGaussianSplatBaseQuadRenderer(const FGaussianSplatBaseQuadRenderer&) = delete;
	FGaussianSplatBaseQuadRenderer(FGaussianSplatBaseQuadRenderer&&) = delete;

	FGaussianSplatBaseQuadRenderer& operator=(const FGaussianSplatBaseQuadRenderer&) = delete;
	FGaussianSplatBaseQuadRenderer& operator=(FGaussianSplatBaseQuadRenderer&&) = delete;


	void Destroy();

	// Get data preprocessed and ready for vertex factory. Should only run on render thread
	void RunPreprocessStage_RenderThread(FRHICommandListImmediate& RHICmdList, const FMatrix& ObjectToWorld, const FMatrix& InView, const FMatrix& InProj, const FVector& InCameraPositionWS, 
		const FVector4& InScreenParam, bool bIsShadowPass, float splatDecimation, float extraSplatScale, float cov2DSqrtKernelSize, bool showSH0, bool showSH1, bool showSH2, bool showSH3, std::shared_ptr<const EvercoastGaussianSplatCSResult> InGaussianData);


	void ReserveRHIResources(uint32_t splatCount);
	void ReleaseRHIResources();

	// For vertex factory parameter to bind
	FShaderResourceViewRHIRef GetSortValueFinalSRV() const
	{
		// TODO: lock
		return m_sortValueFinalSRV;
	}
	FShaderResourceViewRHIRef GetSplatViewSRV() const
	{
		// TODO: lock
		return m_splatViewSRV;
	}

	uint32 GetCurrentReconstructedNumSplats() const
	{
		std::lock_guard<std::recursive_mutex> guard(m_accessMetadataMutex);

		return m_currReconstructedNumSplats;
	}

	int GetCurrentReconstructedFrameIndex() const
	{
		std::lock_guard<std::recursive_mutex> guard(m_accessMetadataMutex);
		return m_currReconFrameIndex;
	}

private:

	

	
	
	uint32_t m_numSplats;
	uint32_t m_maxSplats;

	///////////////// RHI Resources ///////////////// 
	// Input buffers:
	// For quad renderer's sorting(back to front/front to back)
	FBufferRHIRef m_sortKeyListBuffer[GPU_SORT_BUFFER_COUNT];
	FBufferRHIRef m_sortValueListBuffer[GPU_SORT_BUFFER_COUNT];

	// This final sort value buffer exists because GPUSort<->VertexFactory doesn't like each other's data structure
	FBufferRHIRef m_sortValueFinalBuffer;
	FUnorderedAccessViewRHIRef m_sortValueFinalUAV;
	FShaderResourceViewRHIRef m_sortValueFinalSRV;

	// Fort decoding and recon
	FBufferRHIRef m_encodedSplatPositionBuffer;     // position
	FBufferRHIRef m_encodedSplatColourAlphaBuffer;  // colour + alpha
	FBufferRHIRef m_encodedSplatScaleBuffer;		// scale
	FBufferRHIRef m_encodedSplatRotationBuffer;		// rotation
	FBufferRHIRef m_encodedSplatSHCoeffsBuffer;		// SH coeffs
	FBufferRHIRef m_splatViewBuffer;


	// UAV & SRV
	// Sorting
	FUnorderedAccessViewRHIRef m_sortKeyListUAV[GPU_SORT_BUFFER_COUNT];
	FShaderResourceViewRHIRef m_sortKeyListSRV[GPU_SORT_BUFFER_COUNT];
	FUnorderedAccessViewRHIRef m_sortValueListUAV[GPU_SORT_BUFFER_COUNT];
	FShaderResourceViewRHIRef m_sortValueListSRV[GPU_SORT_BUFFER_COUNT]; // <-- Do we really need it? Yes, because it needs to be copied to raw buffer m_sortValueFinalBuffer
	// Decoding and recon
	FShaderResourceViewRHIRef m_encodedSplatPositionSRV;
	FShaderResourceViewRHIRef m_encodedSplatColourAlphaSRV;
	FShaderResourceViewRHIRef m_encodedSplatScaleSRV;
	FShaderResourceViewRHIRef m_encodedSplatRotationSRV;
	FShaderResourceViewRHIRef m_encodedSplatSHCoeffsSRV;

	FUnorderedAccessViewRHIRef m_splatViewUAV;
	FShaderResourceViewRHIRef m_splatViewSRV;

	// LOCKS
	std::recursive_mutex m_accessRHIMutex;
	mutable std::recursive_mutex m_accessMetadataMutex;

	int m_currReconFrameIndex; // for debug purpose only
	uint32_t m_currReconstructedNumSplats; // for keeping num of splats consistent across compute shader and vertex factory


	///////////////// End of RHI Resources ///////////////// 

};

