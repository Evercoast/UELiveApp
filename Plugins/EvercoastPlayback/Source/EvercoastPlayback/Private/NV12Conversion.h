#pragma once
#include "EvercoastHeader.h"
#include "RHIDefinitions.h"
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 2
#include "DataDrivenShaderPlatformInfo.h"
#endif
#endif

struct FNV12TextureConversionVertex
{
	FVector4 Position;
	FVector2D TextureCoordinate;

	FNV12TextureConversionVertex() { }

	FNV12TextureConversionVertex(const FVector4& InPosition, const FVector2D& InTextureCoordinate)
		: Position(InPosition)
		, TextureCoordinate(InTextureCoordinate)
	{ }
};

class FNV12TextureConversionVertexDeclaration :
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
		Elements.Add(FVertexElement(0, 0, VET_Float4, 0, sizeof(FVector4)));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};


/**
 * A dummy vertex buffer to bind when rendering. This prevents some D3D debug warnings
 * about zero-element input layouts but is not strictly required.
 */
class FNV12ConversionDummyIndexBuffer :
	public FIndexBuffer
{
public:

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
#else
	virtual void InitRHI() override
#endif
	{
		// Setup index buffer
		int NumIndices = 6;
#if ENGINE_MAJOR_VERSION == 5
		FRHIResourceCreateInfo CreateInfo(TEXT("NV12ConversionQuadIB"));
#else
		FRHIResourceCreateInfo CreateInfo;
#endif

#if ENGINE_MAJOR_VERSION == 5

#if ENGINE_MINOR_VERSION >= 3
		IndexBufferRHI = RHICmdList.CreateBuffer(sizeof(uint16) * NumIndices, BUF_Static | BUF_IndexBuffer, sizeof(uint16), ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		uint16* pIndices = (uint16*)RHICmdList.LockBuffer(IndexBufferRHI, 0, sizeof(uint16) * NumIndices, RLM_WriteOnly);
#else
		IndexBufferRHI = RHICreateBuffer(sizeof(uint16) * NumIndices, BUF_Static | BUF_IndexBuffer, sizeof(uint16), ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		uint16* pIndices = (uint16*)RHILockBuffer(IndexBufferRHI, 0, sizeof(uint16) * NumIndices, RLM_WriteOnly);
#endif
#else
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), sizeof(uint16) * NumIndices, BUF_Static, CreateInfo);
		void* VoidPtr = RHILockIndexBuffer(IndexBufferRHI, 0, sizeof(uint16) * NumIndices, RLM_WriteOnly);
		uint16* pIndices = reinterpret_cast<uint16*>(VoidPtr);
#endif

		pIndices[0] = 0;
		pIndices[1] = 1;
		pIndices[2] = 2;
		pIndices[3] = 0;
		pIndices[4] = 2;
		pIndices[5] = 3;

#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 3
		RHICmdList.UnlockBuffer(IndexBufferRHI);
#else
		RHIUnlockBuffer(IndexBufferRHI);
#endif
#else
		RHIUnlockIndexBuffer(IndexBufferRHI);
#endif
	}
};


class FNV12ConversionDummyVertexBuffer :
	public FVertexBuffer
{
public:

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
#else
	virtual void InitRHI() override
#endif
	{
#if ENGINE_MAJOR_VERSION == 5
		FRHIResourceCreateInfo CreateInfo(TEXT("NV12ConversionQuadVB"));
#else
		FRHIResourceCreateInfo CreateInfo;
#endif
		void* BufferData = nullptr;

#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 3
		VertexBufferRHI = RHICmdList.CreateBuffer(sizeof(FVector4f) * 4, BUF_Static | BUF_VertexBuffer, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		FVector4* DummyContents = (FVector4*)RHICmdList.LockBuffer(VertexBufferRHI, 0, sizeof(FVector4f) * 4, RLM_WriteOnly);
#else
		VertexBufferRHI = RHICreateBuffer(sizeof(FVector4f) * 4, BUF_Static | BUF_VertexBuffer, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		FVector4* DummyContents = (FVector4*)RHILockBuffer(VertexBufferRHI, 0, sizeof(FVector4f) * 4, RLM_WriteOnly);
#endif
#else
		VertexBufferRHI = RHICreateAndLockVertexBuffer(sizeof(FVector4) * 4, BUF_Static, CreateInfo, BufferData);
		FVector4* DummyContents = (FVector4*)BufferData;
#endif
		
		DummyContents[0] = FVector4(0.f, 0.f, 0.f, 0.f);
		DummyContents[1] = FVector4(1.f, 0.f, 0.f, 0.f);
		DummyContents[2] = FVector4(0.f, 1.f, 0.f, 0.f);
		DummyContents[3] = FVector4(1.f, 1.f, 0.f, 0.f);
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



/** Shaders to render our post process material */
class FNV12TextureConversionVS :
	public FGlobalShader
{
	DECLARE_SHADER_TYPE(FNV12TextureConversionVS, Global);

public:

	FNV12TextureConversionVS() { }
	FNV12TextureConversionVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}
};


class FNV12TextureConversionPS :
	public FGlobalShader
{
	DECLARE_SHADER_TYPE(FNV12TextureConversionPS, Global);

public:

	FNV12TextureConversionPS() {}
	FNV12TextureConversionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		TextureY.Bind(Initializer.ParameterMap, TEXT("TextureY"));
		TextureU.Bind(Initializer.ParameterMap, TEXT("TextureU"));
		TextureV.Bind(Initializer.ParameterMap, TEXT("TextureV"));

		PointClampedSamplerY.Bind(Initializer.ParameterMap, TEXT("PointClampedSamplerY"));
		BilinearClampedSamplerU.Bind(Initializer.ParameterMap, TEXT("BilinearClampedSamplerU"));
		BilinearClampedSamplerV.Bind(Initializer.ParameterMap, TEXT("BilinearClampedSamplerV"));
	}

	void SetParameters(FRHICommandListImmediate& RHICmdList, const FShaderResourceViewRHIRef& InTextureY, const FShaderResourceViewRHIRef& InTextureU, const FShaderResourceViewRHIRef& InTextureV)
	{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3

		FRHIBatchedShaderParameters& Params = RHICmdList.GetScratchShaderParameters();
		SetSRVParameter(Params, TextureY, InTextureY);
		SetSRVParameter(Params, TextureU, InTextureU);
		SetSRVParameter(Params, TextureV, InTextureV);

		SetSamplerParameter(Params, PointClampedSamplerY, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		SetSamplerParameter(Params, BilinearClampedSamplerU, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		SetSamplerParameter(Params, BilinearClampedSamplerV, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		RHICmdList.SetBatchedShaderParameters(RHICmdList.GetBoundPixelShader(), Params);

#else
		SetSRVParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), TextureY, InTextureY);
		SetSRVParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), TextureU, InTextureU);
		SetSRVParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), TextureV, InTextureV);

		SetSamplerParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), PointClampedSamplerY, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		SetSamplerParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), BilinearClampedSamplerU, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		SetSamplerParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), BilinearClampedSamplerV, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
#endif
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, TextureY);
	LAYOUT_FIELD(FShaderResourceParameter, TextureU);
	LAYOUT_FIELD(FShaderResourceParameter, TextureV);
	LAYOUT_FIELD(FShaderResourceParameter, PointClampedSamplerY);
	LAYOUT_FIELD(FShaderResourceParameter, BilinearClampedSamplerU);
	LAYOUT_FIELD(FShaderResourceParameter, BilinearClampedSamplerV);
};
