#pragma once


#include "CoreMinimal.h"
#include "VertexFactory.h"
#include "LocalVertexFactory.h"
#include "Gaussian/GaussianSplatBaseQuadRenderer.h"
#include <memory>
#include <mutex>

class EvercoastGaussianSplatCSResult;

#define GPU_SORT_BUFFER_COUNT (2)
class FEvercoastGaussianSplatBaseVertexFactoryParameters;
class FEvercoastGaussianSplatBaseVertexFactory final : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FEvercoastGaussianSplatBaseVertexFactory);

	friend class FEvercoastGaussianSplatBaseVertexFactoryParameters;
public:
	FEvercoastGaussianSplatBaseVertexFactory(ERHIFeatureLevel::Type InFeatureLevel);

	/**
	 * Constructs render resources for this vertex factory.
	 */
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
#else
	virtual void InitRHI() override;
#endif


	/**
	 * Release render resources for this vertex factory.
	 */
	virtual void ReleaseRHI() override;

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory?
	 */
	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);

	/**
	 * Set parameters for this vertex factory instance.
	 */
	/*
	void SetParameters(const FEvercoastGaussianSplatBaseVertexFactoryParameters& InUniformParameters);

	inline const FUniformBufferRHIRef GetPointCloudVertexFactoryUniformBuffer() const
	{
		return UniformBuffer;
	}
	*/

	// Just reserve RHI resources, no ownership of the data is kept
	void ReserveGaussianSplatRHI(std::shared_ptr<const EvercoastGaussianSplatCSResult> encodedGaussian);

	// Run compute shader to decode and deinterlace the data, then transition the resource to SRV ready for rendering
	void PerformComputeShaderSplatDataReconForQuadRenderer(const FMatrix& ObjectToWorld, const FMatrix& InView, const FMatrix& InProj,
		const FVector& InCameraPositionWS, const FVector4& InScreenParam, bool isShadowPass, float splatDecimation, float splatExtraScale,
		float cov2DSqrtKernelSize, bool showSH0Colour, bool showSH1Colour, bool showSH2Colour, bool showSH3Colour,
		std::shared_ptr<const EvercoastGaussianSplatCSResult> encodedGaussian);

	uint32_t GetCurrentReconstructedNumSplats() const
	{
		return m_quadRenderer->GetCurrentReconstructedNumSplats();
	}

	// FOR DEBUG ONLY
	int GetCurrentReconstructedFrameIndex() const
	{
		return m_quadRenderer->GetCurrentReconstructedFrameIndex();
	}

	uint32_t GetNumOfVertices() const;
	FBufferRHIRef GetVertexBufferRHI() const;

	uint32_t GetNumOfIndices() const;
	FBufferRHIRef GetIndexBufferRHI() const;
	FIndexBuffer* GetIndexBufferPtr() const;

private:

	void ReserveGaussianSplatCount(uint32_t inNumSplats);
	void CreateGaussianSplatRHIResources();

	TSharedPtr<FGaussianSplatBaseQuadRenderer> GetQuadRenderer() const
	{
		return m_quadRenderer; // needs copy value
	}

	TSharedPtr<FGaussianSplatBaseQuadRenderer> m_quadRenderer;

	

	// Maybe we should switch to uniform buffer anyway
	/** Buffers to read from */
	//FUniformBufferRHIRef UniformBuffer;
};
