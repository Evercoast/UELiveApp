#pragma once


#include "CoreMinimal.h"
#include "VertexFactory.h"
#include "LocalVertexFactory.h"
#include <memory>
#include <mutex>

class EvercoastGaussianSplatCSResult;
class FEvercoastGaussianSplatVertexFactoryShaderParameters;

#define GPU_SORT_BUFFER_COUNT (2)
// TODO: from FLocalVertexFactory to FVertexFactory
class FEvercoastGaussianSplatVertexFactory final : public FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FEvercoastGaussianSplatVertexFactory);

public:
	FEvercoastGaussianSplatVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const char* InDebugName);

	// ~Beginning of VertexFactory vtable
	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	// ~End of VertexFactory vtable

	// Beginning of FRenderResource interface.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
#else
	virtual void InitRHI() override;
#endif
	virtual void ReleaseRHI() override;
	// End of FRenderResource interface

	
	// Just reserve RHI resources, no ownership of the data is kept
	void ReserveGaussianSplatRHI(std::shared_ptr<const EvercoastGaussianSplatCSResult> encodedGaussian);

	// Run compute shader to decode and deinterlace the data, then transition the resource to SRV ready for rendering
	void PerformComputeShaderSplatDataReconForQuadRenderer(const FMatrix& ObjectToWorld, const FMatrix& InView, const FMatrix& InProj, 
		const FVector& InCameraPositionWS, const FVector4& InScreenParam, bool isShadowPass, float splatDecimation, float splatExtraScale, 
		float cov2DSqrtKernelSize, bool showSH0Colour, bool showSH1Colour, bool showSH2Colour, bool showSH3Colour,
		std::shared_ptr<const EvercoastGaussianSplatCSResult> encodedGaussian);

	void SetShadowBlobScale(float scale);

	uint32_t GetCurrentReconstructedNumSplats() const
	{
		std::lock_guard<std::recursive_mutex> guard(m_accessMetadataLock);
		return m_currReconstructedNumSplats;
	}

	// FOR DEBUG ONLY
	int GetCurrentReconstructedFrameIndex() const
	{
		std::lock_guard<std::recursive_mutex> guard(m_accessMetadataLock);
		return m_currReconFrameIndex;
	}

private:
	friend class FEvercoastGaussianSplatVertexFactoryShaderParameters;

	void ReserveGaussianSplatCount(uint32_t inNumSplats);
	void CreateGaussianSplatRHIResources();
	void ReleaseGaussianSplatRHIResources();

	// Aggrigate all data here!
	// Metadata
	uint32_t m_numSplats;
	uint32_t m_maxSplats;
	uint32_t m_currSortResultBufferIndex;
	float m_shadowBlobScale;
	
	// Buffers
	// For quad renderer's sorting(back to front)
	FBufferRHIRef m_sortKeyListBuffer[GPU_SORT_BUFFER_COUNT];
	FBufferRHIRef m_sortValueListBuffer[GPU_SORT_BUFFER_COUNT];
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
	FShaderResourceViewRHIRef m_sortValueListSRV[GPU_SORT_BUFFER_COUNT]; // <-- do we really need it?
	// Decoding and recon
	FShaderResourceViewRHIRef m_encodedSplatPositionSRV;
	FShaderResourceViewRHIRef m_encodedSplatColourAlphaSRV;
	FShaderResourceViewRHIRef m_encodedSplatScaleSRV;
	FShaderResourceViewRHIRef m_encodedSplatRotationSRV;
	FShaderResourceViewRHIRef m_encodedSplatSHCoeffsSRV;

	FUnorderedAccessViewRHIRef m_splatViewUAV;
	FShaderResourceViewRHIRef m_splatViewSRV;

	std::recursive_mutex m_accessRHILock;
	mutable std::recursive_mutex m_accessMetadataLock;

	int m_currReconFrameIndex; // for debug purpose only
	uint32_t m_currReconstructedNumSplats; // for keeping num of splats consistent across compute shader and vertex factory
};