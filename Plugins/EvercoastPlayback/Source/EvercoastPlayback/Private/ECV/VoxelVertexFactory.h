// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UnrealEngineCompatibility.h"
#include "RHI.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "VertexFactory.h"

struct EvercoastLocalVoxelFrame;
/**
 * Uniform buffer to hold parameters for point cloud rendering
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT( FVoxelVertexFactoryParameters, )
	SHADER_PARAMETER_SRV(Buffer<int4>, VertexFetch_VoxelPositionBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, VertexFetch_VoxelColorBuffer)
END_GLOBAL_SHADER_PARAMETER_STRUCT()
typedef TUniformBufferRef<FVoxelVertexFactoryParameters> FVoxelVertexFactoryBufferRef;

/**
 * Vertex factory for point cloud rendering. This base version uses the dummy color buffer
 */
class FVoxelVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FVoxelVertexFactory);

public:
	FVoxelVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: FVertexFactory(InFeatureLevel)
	{
	}

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
	* Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	*/
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	/**
	 * Set parameters for this vertex factory instance.
	 */
	void SetParameters(const FVoxelVertexFactoryParameters& InUniformParameters);
	void SetParameters(const FVector3f& InBoundsMin, const float InBoundsDim);

	inline const FUniformBufferRHIRef GetVoxelVertexFactoryUniformBuffer() const
	{
		return UniformBuffer;
	}

	inline const FVector3f& GetBoundsMin() const
	{
		return BoundsMin;
	}

	inline const float& GetBoundsDim() const
	{
		return BoundsDim;
	}

private:
	/** Buffers to read from */
	FUniformBufferRHIRef UniformBuffer;

	FVector3f BoundsMin;
	float BoundsDim;
};
