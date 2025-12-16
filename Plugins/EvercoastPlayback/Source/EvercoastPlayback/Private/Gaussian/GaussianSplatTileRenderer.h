#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "UnrealEngineCompatibility.h"
#include <memory>
#include <vector>
#include <mutex>

#if PLATFORM_WINDOWS

class EvercoastGaussianSplatCSResult;
class UTextureRenderTarget2D;
// Input: array of EvercoastGaussianSplatCSResult, mvp matrices, screen size params
// Output: final image
// First stage result can be used for both tile renderer for soft raster, and quad renderer for alpha blending
class FGaussianSplatTileRenderer
{
public:
	FGaussianSplatTileRenderer();
	virtual ~FGaussianSplatTileRenderer();

	FGaussianSplatTileRenderer(const FGaussianSplatTileRenderer&) = delete;
	FGaussianSplatTileRenderer(FGaussianSplatTileRenderer&&) = delete;

	FGaussianSplatTileRenderer& operator=(const FGaussianSplatTileRenderer& ) = delete;
	FGaussianSplatTileRenderer& operator=(FGaussianSplatTileRenderer&&) = delete;

	void SaveInput(const FMatrix& ObjectToWorld, const FMatrix& InView, const FMatrix& InProj, const FVector& InCameraPositionWS, const FVector4& InScreenParam, float cov2DSqrtKernelSize,
		bool showSH0, bool showSH1, bool showSH2, bool showSH3, const FVector4& InDepthOutputThreshold, std::shared_ptr<const EvercoastGaussianSplatCSResult> InGaussianData);

	FTextureRHIRef GetOutputColourRenderTarget() const;
	FTextureRHIRef GetOutputDepthRenderTarget() const;
	int GetOutputFrameCounter() const;

	int GetSavedFrameCounter() const;
	FVector2f GetSavedOutputRenderTargetUVScale() const;

	void Destroy();
	void DEBUG_TestInclusiveSum(FRHICommandListImmediate& RHICmdList, int32_t TestSize);

	// For view extensions to call
	bool RunPipelineWithLastSavedInput_RenderThread(FRHICommandListImmediate& RHICmdList);
private:
	void ReservePermanentResources();
	void ReserveViewDependentResources(const FVector4& InScreenParam);
	void ReserveFirstStageResources(uint32_t splatCount);
	void ReserveSecondStageResources(uint32_t tileConjugateSplatCount, const FVector4& InScreenParam);

	void ReleasePermanentResources();
	void ReleaseViewDependentResources();
	void ReleaseFirstStageResources();
	void ReleaseSecondStageResources();

	bool RunPipeline_RenderThread(FRHICommandListImmediate& RHICmdList, const FMatrix& ObjectToWorld, const FMatrix& InView, const FMatrix& InProj, const FVector& InCameraPositionWS, const FVector4& InScreenParam, float cov2DSqrtKernelSize,
		bool showSH0, bool showSH1, bool showSH2, bool showSH3, const FVector4& DepthOutputThreshold, std::shared_ptr<const EvercoastGaussianSplatCSResult> InGaussianData);

	uint32_t RunFirstStagePipeline_RenderThread(FRHICommandListImmediate& RHICmdList, const FMatrix& InObjectToWorld, const FMatrix& InView, const FMatrix& InProj, const FVector& InCameraPositionWS,
		uint32_t numSplats, const FVector4& InScreenParam, float InCov2DSqrtKernelSize, bool InShowSH0, bool InShowSH1, bool InShowSH2, bool InShowSH3);
	bool RunSecondStagePipeline_RenderThread(FRHICommandListImmediate& RHICmdList, uint32_t numSplats, uint32_t numTileConjugateSplat, const FVector4& InScreenParam, const FVector4& DepthOutputThreshold);
private:
	// Bookkeeping
	std::shared_ptr<const EvercoastGaussianSplatCSResult> m_encodedGaussianSplats;
	uint32_t m_maxSplats; // reservation
	uint32_t m_maxTileConjugateSplat; // reservation
	uint32_t m_currSortResultBufferIndex;
	uint32_t m_numTileX;
	uint32_t m_numTileY;
	FVector2f m_OutputRenderTargetUVScale;

	int m_savedFrameCounter;

	
	// ALL BUFFERS WILL BE CACHED AND REFUSED FOR PERFORMANCE REASON
	// LOCKS
	mutable std::recursive_mutex m_accessRHIMutex;
	// FIRST STAGE BUFFERS
	// Input buffers
	FBufferRHIRef												m_encodedSplatPositionBuffer;				// position
	FBufferRHIRef												m_encodedSplatColourAlphaBuffer;			// colour + alpha
	FBufferRHIRef												m_encodedSplatScaleBuffer;					// scale
	FBufferRHIRef												m_encodedSplatRotationBuffer;				// rotation
	FBufferRHIRef												m_encodedSplatSHCoeffsBuffer;				// SH coeffs
	// Input buffer SRVs
	FShaderResourceViewRHIRef									m_encodedSplatPositionSRV;
	FShaderResourceViewRHIRef									m_encodedSplatColourAlphaSRV;
	FShaderResourceViewRHIRef									m_encodedSplatScaleSRV;
	FShaderResourceViewRHIRef									m_encodedSplatRotationSRV;
	FShaderResourceViewRHIRef									m_encodedSplatSHCoeffsSRV;
	

	// Preprocessed SplatView buffer
	FBufferRHIRef												m_splatViewBuffer;
	// Preprocessed SplatView UAV and SRV
	FUnorderedAccessViewRHIRef									m_splatViewUAV;
	FShaderResourceViewRHIRef									m_splatViewSRV;

	FBufferRHIRef												m_splatNumTileTouched;						// size=splat count
	FUnorderedAccessViewRHIRef									m_splatNumTileTouchedUAV;
	FShaderResourceViewRHIRef									m_splatNumTileTouchedSRV;
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// SECOND STAGE BUFFERS
	// Intermediate conjugated buffers
	FBufferRHIRef												m_tileConjugateSplatOffset;					// size=splat count
	// Intermediate conjugated buffer UAVs
	FUnorderedAccessViewRHIRef									m_tileConjugateSplatOffsetUAV;
	FShaderResourceViewRHIRef									m_tileConjugateSplatOffsetSRV;

	// Inclusive sum buffers
	FBufferRHIRef												m_intermInclusiveSumGroupSumsBuffer;		// size=4096
	FBufferRHIRef												m_intermInclusiveSumGroupSumsScanBuffer;	// size=4096
	// Inclusive sum buffer UAVs
	FUnorderedAccessViewRHIRef									m_intermInclusiveSumGroupSumsUAV;
	FShaderResourceViewRHIRef									m_intermInclusiveSumGroupSumsSRV;
	FUnorderedAccessViewRHIRef									m_intermInclusiveSumGroupSumsScanUAV;
	FShaderResourceViewRHIRef									m_intermInclusiveSumGroupSumsScanSRV;

	// Radix sorting buffers
	FBufferRHIRef												m_tileConjugateSplatKeys[2];				// size=m_numTileConjugateSplat
	FBufferRHIRef												m_tileConjugateSplatValues[2];				// size=m_numTileConjugateSplat
	// Radix sorting buffer UAVs
	FUnorderedAccessViewRHIRef									m_tileConjugateSplatKeysUAV[2];
	FUnorderedAccessViewRHIRef									m_tileConjugateSplatValuesUAV[2];
	// Radix sorting buffer SRVs
	FShaderResourceViewRHIRef									m_tileConjugateSplatKeysSRV[2];
	FShaderResourceViewRHIRef									m_tileConjugateSplatValuesSRV[2];

	// Range buffer
	FBufferRHIRef												m_tileToSplatRanges;						// size=tileX * tileY
	// Range buffer UAV and SRV
	FUnorderedAccessViewRHIRef									m_tileToSplatRangesUAV;
	FShaderResourceViewRHIRef									m_tileToSplatRangesSRV;

	// Output GPU textures(used to be render targets)
	FTextureRHIRef												m_outputColourRenderTarget;
	FTextureRHIRef												m_outputDepthRenderTarget;
	// UAV
	FUnorderedAccessViewRHIRef									m_outputColourRenderTargetUAV;
	FUnorderedAccessViewRHIRef									m_outputDepthRenderTargetUAV;

	int															m_outputFrameCounter;

	// FOR SAVE INPUT
	FMatrix SavedObjectToWorld;
	FMatrix SavedView;
	FMatrix SavedProj;
	FVector SavedCameraPositionWS;
	FVector4 SavedScreenParam;
	float SavedCov2DSqrtKernelSize;
	bool SavedShowSH0;
	bool SavedShowSH1;
	bool SavedShowSH2;
	bool SavedShowSH3;
	FVector4 SavedDepthOutputThreshold;
	std::shared_ptr<const EvercoastGaussianSplatCSResult> SavedEncodedGaussianSplats;
	FVector2f SavedOutputRenderTargetUVScale;
};

#endif