#pragma once

#include "CoreMinimal.h"
#include "VertexFactory.h"
#include "LocalVertexFactory.h"
#include <memory>

class FEvercoastVoxelSceneProxy;
class FEvercoastInstancedCubeVertexFactoryShaderParameters;
struct EvercoastLocalVoxelFrame;
///////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEvercoastInstancedCubeVertexFactory
///////////////////////////////////////////////////////////////////////////////////////////////////////////
class FEvercoastInstancedCubeVertexFactory final : public FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FEvercoastInstancedCubeVertexFactory);
public:
	FEvercoastInstancedCubeVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const char* InDebugName);

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

	void SetInstancingData(std::shared_ptr<EvercoastLocalVoxelFrame> voxelFrame);
	std::shared_ptr<EvercoastLocalVoxelFrame> GetInstancingData() const;

	FShaderResourceViewRHIRef GetPositionBufferSRV() const
	{
		return m_positionsSRV;
	}

private:
	friend class FEvercoastInstancedCubeVertexFactoryShaderParameters;

	std::shared_ptr<EvercoastLocalVoxelFrame> m_voxelFrame;

	TArray<uint64_t>				m_positions;
	FTexture2DRHIRef				m_positionsTex;
	FShaderResourceViewRHIRef		m_positionsSRV;
	TArray<uint32_t>				m_colours;
	FTexture2DRHIRef				m_coloursTex;
	FShaderResourceViewRHIRef		m_coloursSRV;
};