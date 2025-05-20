#pragma once


#include "CoreMinimal.h"
#include "VertexFactory.h"
#include "LocalVertexFactory.h"
#include <memory>

class EvercoastGaussianSplatPassthroughResult;
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

	
	void SetEncodedGaussianSplatData(std::shared_ptr<const EvercoastGaussianSplatPassthroughResult> encodedGaussian);

	// Run compute shader to decode and deinterlace the data, then transition the resource to SRV ready for rendering
	//void SaveEssentialMatrices(const FMatrix& ObjectToWorld, const FMatrix& ViewProj);
	void PerformComputeShaderSplatDataRecon(const FMatrix& ObjectToWorld, const FVector& InPreViewTranslation, const FMatrix& ViewProj, const FMatrix& InView, const FMatrix& InProj, const FVector4& InScreenParam, const FMatrix& InClipToWorld, bool isShadowPass);
private:
	friend class FEvercoastGaussianSplatVertexFactoryShaderParameters;

	void ReserveGaussianSplatCount(uint32_t inNumSplats);
	void CreateGaussianSplatRHIResources();
	void ReleaseGaussianSplatRHIResources();
	// TODO: aggrigate all data here!
	// Metadata
	uint32_t m_numSplats;
	uint32_t m_maxSplats;
	uint32_t m_currSortResultBufferIndex;

	// Buffers
	// For sorting
	FBufferRHIRef m_sortKeyListBuffer[GPU_SORT_BUFFER_COUNT];
	FBufferRHIRef m_sortValueListBuffer[GPU_SORT_BUFFER_COUNT];
	// Fort decoding and recon
	FBufferRHIRef m_encodedSplatPositionBuffer;     // position
	FBufferRHIRef m_encodedSplatColourAlphaBuffer;  // colour + alpha
	FBufferRHIRef m_encodedSplatScaleBuffer;		// scale
	FBufferRHIRef m_encodedSplatRotationBuffer;		// rotation
	// TODO: encoded SH buffer here
	FBufferRHIRef m_splatViewBuffer;

	// UAV & SRV
	// Sorting
	FUnorderedAccessViewRHIRef m_sortKeyListUAV[GPU_SORT_BUFFER_COUNT];
	FShaderResourceViewRHIRef m_sortKeyListSRV[GPU_SORT_BUFFER_COUNT];
	FUnorderedAccessViewRHIRef m_sortValueListUAV[GPU_SORT_BUFFER_COUNT];
	FShaderResourceViewRHIRef m_sortValueListSRV[GPU_SORT_BUFFER_COUNT]; // <-- do we really need it?
	// Decoding and recon
	FUnorderedAccessViewRHIRef m_encodedSplatPositionUAV;
	FUnorderedAccessViewRHIRef m_encodedSplatColourAlphaUAV;
	FUnorderedAccessViewRHIRef m_encodedSplatScaleUAV;
	FUnorderedAccessViewRHIRef m_encodedSplatRotationUAV;
	FUnorderedAccessViewRHIRef m_splatViewUAV;
	FShaderResourceViewRHIRef m_splatViewSRV;

	// retained raw encoded data block
	std::shared_ptr<const EvercoastGaussianSplatPassthroughResult> m_encodedGaussian;
};