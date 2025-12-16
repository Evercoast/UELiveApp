#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "UnrealEngineCompatibility.h"
#include "Gaussian/GaussianSplatComputeShaderConstants.h"
#include <memory>
#include <vector>
#include <mutex>


class EvercoastGaussianSplatCSResult;
class FGaussianSplatOffscreenQuadRenderer
{
public:
	FGaussianSplatOffscreenQuadRenderer();
	virtual ~FGaussianSplatOffscreenQuadRenderer();

	FGaussianSplatOffscreenQuadRenderer(const FGaussianSplatOffscreenQuadRenderer&) = delete;
	FGaussianSplatOffscreenQuadRenderer(FGaussianSplatOffscreenQuadRenderer&&) = delete;

	FGaussianSplatOffscreenQuadRenderer& operator=(const FGaussianSplatOffscreenQuadRenderer&) = delete;
	FGaussianSplatOffscreenQuadRenderer& operator=(FGaussianSplatOffscreenQuadRenderer&&) = delete;


	void SaveInput(const FMatrix& ObjectToWorld, const FMatrix& InView, const FMatrix& InProj, const FVector& InCameraPositionWS, const FVector4& InScreenParam, float extraSplatScale, float cov2DSqrtKernelSize,
		bool showSH0, bool showSH1, bool showSH2, bool showSH3, const FVector4& InDepthOutputThreshold, std::shared_ptr<const EvercoastGaussianSplatCSResult> InGaussianData);

	// For view extensions to call
	bool RunPipelineWithLastSavedInput_RenderThread(FRHICommandListImmediate& RHICmdList);

	FTextureRHIRef GetOutputColourRenderTarget() const;
	FTextureRHIRef GetOutputDepthRenderTarget() const;
	FVector2f GetSavedOutputRenderTargetUVScale() const;

	void Destroy();
private:

	void ReserveResources(uint32_t splatCount, const FVector4& InScreenParam);
	void ReleaseResources();

	bool RunPipeline_RenderThread(FRHICommandListImmediate& RHICmdList, const FMatrix& ObjectToWorld, const FMatrix& InView, const FMatrix& InProj, const FVector& InCameraPositionWS, const FVector4& InScreenParam, 
		float extraSplatScale, float cov2DSqrtKernelSize, bool showSH0, bool showSH1, bool showSH2, bool showSH3, const FVector4& InDepthOutputThreshold, std::shared_ptr<const EvercoastGaussianSplatCSResult> InGaussianData);

	uint32_t RunPreprocessStage(FRHICommandListImmediate& RHICmdList, const FMatrix& InObjectToWorld, const FMatrix& InView, const FMatrix& InProj, const FVector& InCameraPositionWS,
		uint32_t numSplats, const FVector4& InScreenParam, float InExtraSplatScale, float InCov2DSqrtKernelSize, bool InShowSH0, bool InShowSH1, bool InShowSH2, bool InShowSH3);

	void RunRenderStage(FRHICommandListImmediate& RHICmdList, uint32_t numSplats, const FVector4& currScreenParam, const FVector4& InDepthOutputThreshold);

	// LOCK
	mutable std::recursive_mutex m_accessRHIMutex;

	// Bookkeeping
	std::shared_ptr<const EvercoastGaussianSplatCSResult> m_encodedGaussianSplats;
	uint32_t m_maxSplats; // reservation

	///////////////// RHI Resources ///////////////// 
	// Input buffers:
	FBufferRHIRef												m_encodedSplatPositionBuffer;				// position
	FBufferRHIRef												m_encodedSplatColourAlphaBuffer;			// colour + alpha
	FBufferRHIRef												m_encodedSplatScaleBuffer;					// scale
	FBufferRHIRef												m_encodedSplatRotationBuffer;				// rotation
	FBufferRHIRef												m_encodedSplatSHCoeffsBuffer;				// SH coeffs
	// Input buffer SRVs:
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

	// Sorting buffers
	// For quad renderer's sorting(back to front)
	FBufferRHIRef												m_sortKeyListBuffer[GPU_SORT_BUFFER_COUNT];
	FBufferRHIRef												m_sortValueListBuffer[GPU_SORT_BUFFER_COUNT];
	
	// Sorting buffer SRV/UAVs
	FUnorderedAccessViewRHIRef									m_sortKeyListUAV[GPU_SORT_BUFFER_COUNT];
	FShaderResourceViewRHIRef									m_sortKeyListSRV[GPU_SORT_BUFFER_COUNT];
	FUnorderedAccessViewRHIRef									m_sortValueListUAV[GPU_SORT_BUFFER_COUNT];
	FShaderResourceViewRHIRef									m_sortValueListSRV[GPU_SORT_BUFFER_COUNT];


	// This final sort value buffer exists because on Mac GPUSort<->VertexFactory doesn't like each other's data structure
	FBufferRHIRef												m_sortValueFinalBuffer;
	FUnorderedAccessViewRHIRef									m_sortValueFinalUAV;
	FShaderResourceViewRHIRef									m_sortValueFinalSRV;


	// Output GPU textures
	FTextureRHIRef												m_outputColourRenderTarget; // <- intermediate
	FTextureRHIRef												m_outputColourRenderTargetFinal; // <- final
	FTextureRHIRef												m_outputDepthRenderTarget;
	FTextureRHIRef												m_depthStencilTarget; // for depth image generation for offscreen quad renderer
	///////////////// End of RHI Resources ///////////////// 

	// FOR SAVE INPUT
	FMatrix SavedObjectToWorld;
	FMatrix SavedView;
	FMatrix SavedProj;
	FVector SavedCameraPositionWS;
	FVector4 SavedScreenParam;
	float SavedExtraSplatScale;
	float SavedCov2DSqrtKernelSize;
	bool SavedShowSH0;
	bool SavedShowSH1;
	bool SavedShowSH2;
	bool SavedShowSH3;
	FVector4 SavedDepthOutputThreshold;
	std::shared_ptr<const EvercoastGaussianSplatCSResult> SavedEncodedGaussianSplats;
	FVector2f SavedOutputRenderTargetUVScale;
};

