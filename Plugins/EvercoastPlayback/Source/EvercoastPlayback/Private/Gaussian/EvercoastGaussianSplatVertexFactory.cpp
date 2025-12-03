#include "Gaussian/EvercoastGaussianSplatVertexFactory.h"
#include "EvercoastVoxelDecoder.h" // log define
#include "ShaderCompilerCore.h"
#include "MeshDrawShaderBindings.h"
#include "RenderUtils.h"
#include "MeshMaterialShader.h"
#include "Gaussian/EvercoastGaussianSplatCSResult.h"
#include "Gaussian/GaussianSplatPreprocessComputeShader.h"
#include "Gaussian/InclusiveSumComputeShader.h"
#include "Gaussian/GaussianSplatAuxiliaryComputeShader.h"
#include "Gaussian/GaussianSplatComputeShaderConstants.h"
#include "Gaussian/GaussianSplatQuadRendererComputeShader.h"
#include "Gaussian/GaussianSplatBaseQuadRenderer.h"

#include "MaterialShared.h"
#include "GPUSort.h"
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 2
#include "MaterialDomain.h"
#endif
#endif



class FEvercoastGaussianSplatBaseVertexFactoryParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FEvercoastGaussianSplatBaseVertexFactoryParameters, NonVirtual);

public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		NumSplats.Bind(ParameterMap, TEXT("_NumSplats"), SPF_Mandatory);
        SortValueFinal.Bind(ParameterMap, TEXT("_SortValueFinal"), SPF_Mandatory);
		SplatViewSRV.Bind(ParameterMap, TEXT("_SplatViewData"), SPF_Mandatory);
	}

	void GetElementShaderBindings(
		const class FSceneInterface* Scene,
		const FSceneView* View,
		const class FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams
	) const
	{
		const FEvercoastGaussianSplatBaseVertexFactory* GaussianSplatVertexFactory = ((const FEvercoastGaussianSplatBaseVertexFactory*)VertexFactory);

		/*
		FRHIUniformBuffer* VertexFactoryUniformBuffer = static_cast<FRHIUniformBuffer*>(BatchElement.VertexFactoryUserData);

		if (!VertexFactoryUniformBuffer)
		{
			// No batch element override
			VertexFactoryUniformBuffer = GaussianSplatVertexFactory->GetUniformBuffer();
		}
		// Bind default local vertex factory uniforms
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FLocalVertexFactoryUniformShaderParameters>(), VertexFactoryUniformBuffer);
		*/

		// bind vertex factory's SRV of RHIBuffer to relavant shader parameter
		int reconNumSplats = GaussianSplatVertexFactory->GetQuadRenderer()->GetCurrentReconstructedNumSplats();
		ShaderBindings.Add(NumSplats, reconNumSplats);

        ShaderBindings.Add(SortValueFinal, GaussianSplatVertexFactory->GetQuadRenderer()->GetSortValueFinalSRV());
		ShaderBindings.Add(SplatViewSRV, GaussianSplatVertexFactory->GetQuadRenderer()->GetSplatViewSRV());
	}
private:
	LAYOUT_FIELD(FShaderParameter, NumSplats);
    LAYOUT_FIELD(FShaderResourceParameter, SortValueFinal);
	LAYOUT_FIELD(FShaderResourceParameter, SplatViewSRV);
};


IMPLEMENT_TYPE_LAYOUT(FEvercoastGaussianSplatBaseVertexFactoryParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FEvercoastGaussianSplatBaseVertexFactory, SF_Vertex, FEvercoastGaussianSplatBaseVertexFactoryParameters);

#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 5
IMPLEMENT_VERTEX_FACTORY_TYPE(FEvercoastGaussianSplatBaseVertexFactory, "/EvercoastShaders/GaussianSplatBaseVertexFactory_5_5.ush",
	EVertexFactoryFlags::UsedWithMaterials);

#elif ENGINE_MINOR_VERSION >= 2
IMPLEMENT_VERTEX_FACTORY_TYPE(FEvercoastGaussianSplatBaseVertexFactory, "/EvercoastShaders/GaussianSplatBaseVertexFactory_5_2.ush",
	EVertexFactoryFlags::UsedWithMaterials);
#else
#error Gaussian Splat needs Unreal Engine 5.2 and above!
#endif
#else
#error Gaussian Splat needs Unreal Engine 5.2 and above!
#endif

class FGaussianSplatQuadVertexDeclaration :
	public FRenderResource
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


TGlobalResource<FGaussianSplatQuadVertexDeclaration> GGaussianSplatQuadVertexDeclaration;

class FGaussianSplatQuadVertexBuffer : public FVertexBuffer
{
public:
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
#else
	virtual void InitRHI() override
#endif
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("FGaussianSplatQuadVertexBuffer"));
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		VertexBufferRHI = RHICmdList.CreateBuffer(sizeof(FVector4f) * GetNumOfQuadVertices(), BUF_Static | BUF_VertexBuffer, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		FVector4f* QuadContents = (FVector4f*)RHICmdList.LockBuffer(VertexBufferRHI, 0, sizeof(FVector4f) * 4, RLM_WriteOnly);
#else
		VertexBufferRHI = RHICreateBuffer(sizeof(FVector4f) * GetNumOfQuadVertices(), BUF_Static | BUF_VertexBuffer, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		FVector4f* QuadContents = (FVector4f*)RHILockBuffer(VertexBufferRHI, 0, sizeof(FVector4f) * 4, RLM_WriteOnly);
#endif
		
		// Double sized quad
		QuadContents[0] = FVector4f(-2.0, 0, -2.0, 1.0f);
		QuadContents[1] = FVector4f(+2.0, 0, -2.0, 1.0f);
		QuadContents[2] = FVector4f(-2.0, 0, +2.0, 1.0f);
		QuadContents[3] = FVector4f(+2.0, 0, +2.0, 1.0f);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		RHICmdList.UnlockBuffer(VertexBufferRHI);
#else
		RHIUnlockBuffer(VertexBufferRHI);
#endif
	}

	uint32 GetNumOfQuadVertices() const
	{
		return 4;
	}
};

TGlobalResource<FGaussianSplatQuadVertexBuffer> GGaussianSplatQuadVertexBuffer;


class FGaussianSplatQuadIndexBuffer : public FIndexBuffer
{
public:

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
#else
	virtual void InitRHI() override
#endif

	{
		FRHIResourceCreateInfo CreateInfo(TEXT("FGaussianSplatQuadIndexBuffer"));
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		IndexBufferRHI = RHICmdList.CreateIndexBuffer(sizeof(uint32), GetNumOfQuadIndices() * sizeof(uint32_t), BUF_Static, CreateInfo);
		UpdateRHI(RHICmdList);
#else
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint32), GetNumOfQuadIndices() * sizeof(uint32_t), BUF_Static, CreateInfo);
		UpdateRHI();
#endif
		
	}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	void UpdateRHI(FRHICommandListBase& RHICmdList)
#else
	void UpdateRHI()
#endif
	{
		// Copy the index data into the index buffer.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		uint32_t* Indices = (uint32_t*)RHICmdList.LockBuffer(IndexBufferRHI, 0, GetNumOfQuadIndices() * sizeof(uint32_t), RLM_WriteOnly);
#else
		uint32_t* Indices = (uint32_t*)RHILockBuffer(IndexBufferRHI, 0, GetNumOfQuadIndices() * sizeof(uint32_t), RLM_WriteOnly);
#endif
		Indices[0] = 0;
		Indices[1] = 1;
		Indices[2] = 2;
		Indices[3] = 3;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		RHICmdList.UnlockBuffer(IndexBufferRHI);
#else
		RHIUnlockBuffer(IndexBufferRHI);
#endif
	}

	uint32 GetNumOfQuadIndices() const
	{
		return 4;
	}

};

TGlobalResource<FGaussianSplatQuadIndexBuffer> GGaussianSplatQuadIndexBuffer;

FEvercoastGaussianSplatBaseVertexFactory::FEvercoastGaussianSplatBaseVertexFactory(ERHIFeatureLevel::Type InFeatureLevel) :
	FVertexFactory(InFeatureLevel),
	m_quadRenderer(new FGaussianSplatBaseQuadRenderer())

{
}

uint32_t FEvercoastGaussianSplatBaseVertexFactory::GetNumOfVertices() const
{
	return GGaussianSplatQuadVertexBuffer.GetNumOfQuadVertices();
}

FBufferRHIRef FEvercoastGaussianSplatBaseVertexFactory::GetVertexBufferRHI() const
{
	return GGaussianSplatQuadVertexBuffer.VertexBufferRHI;
}

uint32_t FEvercoastGaussianSplatBaseVertexFactory::GetNumOfIndices() const
{
	return GGaussianSplatQuadIndexBuffer.GetNumOfQuadIndices();
}

FBufferRHIRef FEvercoastGaussianSplatBaseVertexFactory::GetIndexBufferRHI() const
{
	return GGaussianSplatQuadIndexBuffer.IndexBufferRHI;
}

FIndexBuffer* FEvercoastGaussianSplatBaseVertexFactory::GetIndexBufferPtr() const
{
	return &GGaussianSplatQuadIndexBuffer;
}

void FEvercoastGaussianSplatBaseVertexFactory::ReserveGaussianSplatRHI(std::shared_ptr<const EvercoastGaussianSplatCSResult> encodedGaussian)
{
	if (encodedGaussian)
	{
		// reserve splat RHI data and create them if necessary
		m_quadRenderer->ReserveRHIResources(encodedGaussian->pointCount);
	}
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
void FEvercoastGaussianSplatBaseVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
{
	FVertexFactory::InitRHI(RHICmdList);
#else
void FEvercoastGaussianSplatBaseVertexFactory::InitRHI()
{
	FVertexFactory::InitRHI();
#endif
	// Vertex stream
	FVertexStream Stream;

	// No streams should currently exist.
	check(Streams.Num() == 0);

	Stream.VertexBuffer = &GGaussianSplatQuadVertexBuffer;
	Stream.Stride = sizeof(FVector4f);
	Stream.Offset = 0;
	Streams.Add(Stream);

	check(IsValidRef(GGaussianSplatQuadVertexDeclaration.VertexDeclarationRHI));
	SetDeclaration(GGaussianSplatQuadVertexDeclaration.VertexDeclarationRHI);

	// create dummy resource for just 1 splat

	check(IsInRenderingThread());
	m_quadRenderer->ReserveRHIResources(1);

}


void FEvercoastGaussianSplatBaseVertexFactory::ReleaseRHI()
{
	m_quadRenderer->ReleaseRHIResources();

	FVertexFactory::ReleaseRHI();
}

bool FEvercoastGaussianSplatBaseVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	//return RHISupportsManualVertexFetch(Parameters.Platform);
	if (Parameters.MaterialParameters.MaterialDomain == MD_Surface ||
		Parameters.MaterialParameters.bIsDefaultMaterial)
		return true;
	return false;
}

void FEvercoastGaussianSplatBaseVertexFactory::PerformComputeShaderSplatDataReconForQuadRenderer(const FMatrix& ObjectToWorld, const FMatrix& InView, const FMatrix& InProj,
	const FVector& InCameraPositionWS, const FVector4& InScreenParam, bool isShadowPass, float splatDecimation, float splatExtraScale, float cov2DSqrtKernelSize,
	bool showSH0Colour, bool showSH1Colour, bool showSH2Colour, bool showSH3Colour, std::shared_ptr<const EvercoastGaussianSplatCSResult> encodedGaussian)
{
	if (!encodedGaussian)
		return;

	ENQUEUE_RENDER_COMMAND(FDispatchGaussianSplatCompute)([
		quadRenderer = m_quadRenderer,
		ObjectToWorld,
		InView,
		InProj,
		InCameraPositionWS,
		InScreenParam,
		isShadowPass,
		splatDecimation,
		splatExtraScale,
		cov2DSqrtKernelSize,
		showSH0Colour,
		showSH1Colour,
		showSH2Colour,
		showSH3Colour, 
		encodedGaussian
			](FRHICommandListImmediate& RHICmdList)
		{
			quadRenderer->RunPreprocessStage_RenderThread(RHICmdList, ObjectToWorld, InView, InProj, InCameraPositionWS, InScreenParam,
				isShadowPass, splatDecimation, splatExtraScale, cov2DSqrtKernelSize,  showSH0Colour, showSH1Colour, showSH2Colour, showSH3Colour, encodedGaussian);
		}
	);
}
