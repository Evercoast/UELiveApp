// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVFVoxelVertexFactory.h"
#include "RHIStaticStates.h"
#include "SceneManagement.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "ShaderParameterUtils.h"
#include "MeshMaterialShader.h"
#include "MeshDrawShaderBindings.h"
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 2
#include "DataDrivenShaderPlatformInfo.h"
#endif
#endif

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FMVFVoxelVertexFactoryParameters, "VoxelVF");

/**
 * Shader parameters for the vector field visualization vertex factory.
 */
class FMVFVoxelVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FMVFVoxelVertexFactoryShaderParameters, NonVirtual);
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		BoundsMin.Bind(ParameterMap, TEXT("BoundsMin"));
		BoundsDim.Bind(ParameterMap, TEXT("BoundsDim"));
	}

	void GetElementShaderBindings(
		const class FSceneInterface* Scene,
		const class FSceneView* View,
		const class FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const class FVertexFactory* InVertexFactory,
		const struct FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const
	{		
		FMVFVoxelVertexFactory* VertexFactory = (FMVFVoxelVertexFactory*)InVertexFactory;

		ShaderBindings.Add(Shader->GetUniformBufferParameter<FMVFVoxelVertexFactoryParameters>(), VertexFactory->GetVoxelVertexFactoryUniformBuffer());

		ShaderBindings.Add(BoundsMin, VertexFactory->GetBoundsMin());
		ShaderBindings.Add(BoundsDim, VertexFactory->GetBoundsDim());
	}

private:
	LAYOUT_FIELD(FShaderParameter, BoundsMin);
	LAYOUT_FIELD(FShaderParameter, BoundsDim);
};

/**
 * Vertex declaration for point clouds. Verts aren't used so this is to make complaints go away
 */
class FVoxelVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
#else
	virtual void InitRHI() override
#endif
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, 0, VET_Float4, 0, sizeof(FVector4f)));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};
TGlobalResource<FVoxelVertexDeclaration> GVoxelVertexDeclaration;

/**
 * A dummy vertex buffer to bind when rendering point clouds. This prevents
 * some D3D debug warnings about zero-element input layouts but is not strictly
 * required.
 */
class FDummyVertexBuffer :
	public FVertexBuffer
{
public:

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
#else
	virtual void InitRHI() override
#endif
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("FDummyVertexBuffer"));
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 3
		VertexBufferRHI = RHICmdList.CreateBuffer(sizeof(FVector4f) * 4, BUF_Static | BUF_VertexBuffer, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		FVector4f* DummyContents = (FVector4f*)RHICmdList.LockBuffer(VertexBufferRHI, 0, sizeof(FVector3f) * 4, RLM_WriteOnly);
#else
		VertexBufferRHI = RHICreateBuffer(sizeof(FVector4f) * 4, BUF_Static | BUF_VertexBuffer, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		FVector4f* DummyContents = (FVector4f*)RHILockBuffer(VertexBufferRHI, 0, sizeof(FVector3f) * 4, RLM_WriteOnly);
#endif
#else
		void* BufferData = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(sizeof(FVector4) * 4, BUF_Static, CreateInfo, BufferData);
		FVector4* DummyContents = (FVector4*)BufferData;
#endif
//@todo - joeg do I need a quad's worth?
		DummyContents[0] = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		DummyContents[1] = FVector4f(1.0f, 0.0f, 0.0f, 0.0f);
		DummyContents[2] = FVector4f(0.0f, 1.0f, 0.0f, 0.0f);
		DummyContents[3] = FVector4f(1.0f, 1.0f, 0.0f, 0.0f);
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 3
		RHICmdList.UnlockBuffer(VertexBufferRHI);
#else
		RHIUnlockBuffer(VertexBufferRHI);
#endif
#else
		RHIUnlockVertexBuffer(VertexBufferRHI);
#endif
	}
};
TGlobalResource<FDummyVertexBuffer> GVoxelCloudVertexBuffer;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
void FMVFVoxelVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
#else
void FMVFVoxelVertexFactory::InitRHI()
#endif
{
	FVertexStream Stream;

	// No streams should currently exist.
	check( Streams.Num() == 0 );

	Stream.VertexBuffer = &GVoxelCloudVertexBuffer;
	Stream.Stride = sizeof(FVector4f);
	Stream.Offset = 0;
	Streams.Add(Stream);

	// Set the declaration.
	check(IsValidRef(GVoxelVertexDeclaration.VertexDeclarationRHI));
	SetDeclaration(GVoxelVertexDeclaration.VertexDeclarationRHI);
}

void FMVFVoxelVertexFactory::ReleaseRHI()
{
	UniformBuffer.SafeRelease();
	FVertexFactory::ReleaseRHI();
}

bool FMVFVoxelVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	// We exclusively use manual fetch, so we need that supported
	return RHISupportsManualVertexFetch(Parameters.Platform);
}

void FMVFVoxelVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
#if ENGINE_MAJOR_VERSION == 5
	OutEnvironment.SetDefine(TEXT("EVERCOAST_ON_UE5"), TEXT("1"));
#else
	OutEnvironment.SetDefine(TEXT("EVERCOAST_ON_UE4"), TEXT("1"));
#endif
}

void FMVFVoxelVertexFactory::SetParameters(const FMVFVoxelVertexFactoryParameters& InUniformParameters)
{
	UniformBuffer = FMVFVoxelVertexFactoryBufferRef::CreateUniformBufferImmediate(InUniformParameters, UniformBuffer_MultiFrame);
}

void FMVFVoxelVertexFactory::SetParameters(const FVector3f& InBoundsMin, const float InBoundsDim)
{
	BoundsMin = InBoundsMin;
	BoundsDim = InBoundsDim;
}

IMPLEMENT_TYPE_LAYOUT(FMVFVoxelVertexFactoryShaderParameters);

#if ENGINE_MAJOR_VERSION == 5
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FMVFVoxelVertexFactory, SF_Vertex, FMVFVoxelVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_TYPE(FMVFVoxelVertexFactory, "/EvercoastShaders/EvercoastVoxelVertexFactory.ush", 
	EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrimitiveIdStream
	| EVertexFactoryFlags::SupportsPositionOnly);
#else
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FMVFVoxelVertexFactory, SF_Vertex, FMVFVoxelVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_TYPE_EX(FMVFVoxelVertexFactory, "/EvercoastShaders/EvercoastVoxelVertexFactory.ush", true, false, true, true, true, false, false);

#endif