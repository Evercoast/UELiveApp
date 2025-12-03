#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h" // need this to BEGIN_SHADER_PARAMETER_STRUCT
#include "RHIStaticStates.h"
#include "ShaderParameterUtils.h"
#include "UnrealEngineCompatibility.h"


class FOffscreenGaussianQuadVertexDeclaration :
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



class FOffscreenGaussianQuadIndexBuffer :
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
		FRHIResourceCreateInfo CreateInfo(TEXT("OffscreenGaussianQuadIB"));
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
		pIndices[1] = 2;
		pIndices[2] = 1;
		pIndices[3] = 1;
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


class FOffscreenGaussianQuadVertexBuffer :
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
		FRHIResourceCreateInfo CreateInfo(TEXT("OffscreenGaussianQuadVB"));
#else
		FRHIResourceCreateInfo CreateInfo;
#endif
		void* BufferData = nullptr;

#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 3
		VertexBufferRHI = RHICmdList.CreateBuffer(sizeof(FVector4f) * 4, BUF_Static | BUF_VertexBuffer, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		FVector4f* VertexContents = (FVector4f*)RHICmdList.LockBuffer(VertexBufferRHI, 0, sizeof(FVector4f) * 4, RLM_WriteOnly);
#else
		VertexBufferRHI = RHICreateBuffer(sizeof(FVector4f) * 4, BUF_Static | BUF_VertexBuffer, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		FVector4f* VertexContents = (FVector4f*)RHILockBuffer(VertexBufferRHI, 0, sizeof(FVector4f) * 4, RLM_WriteOnly);
#endif
#else
		VertexBufferRHI = RHICreateAndLockVertexBuffer(sizeof(FVector4f) * 4, BUF_Static, CreateInfo, BufferData);
		FVector4f* VertexContents = (FVector4f*)BufferData;
#endif

		VertexContents[0] = FVector4f(-1.f, -1.f, 0.0f, 0.0f);
		VertexContents[1] = FVector4f(1.f, -1.f, 0.0f, 0.0f);
		VertexContents[2] = FVector4f(-1.f, 1.f, 0.0f, 0.0f);
		VertexContents[3] = FVector4f(1.f, 1.f, 0.0f, 0.0f);
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


class FOffscreenGaussianQuadVS : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FOffscreenGaussianQuadVS)
    SHADER_USE_PARAMETER_STRUCT(FOffscreenGaussianQuadVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, NumSplats)
        SHADER_PARAMETER_SRV(ByteAddressBuffer, SortValueList)
        SHADER_PARAMETER_SRV(StructuredBuffer<SplatView>, TheSplatViewData)
    END_SHADER_PARAMETER_STRUCT()
public:
    static bool ShouldCompilePermutation(const FShaderPermutationParameters& Params)
    {
        return true;
    }
};

class FOffscreenGaussianQuadPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOffscreenGaussianQuadPS)
    SHADER_USE_PARAMETER_STRUCT(FOffscreenGaussianQuadPS, FGlobalShader)

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER(FVector4f, VecScreenParams)
    END_SHADER_PARAMETER_STRUCT()
public:
    static bool ShouldCompilePermutation(const FShaderPermutationParameters& Params)
    {
        return true;
    }
};


class FOffscreenGaussianQuadFastVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOffscreenGaussianQuadFastVS)
	SHADER_USE_PARAMETER_STRUCT(FOffscreenGaussianQuadFastVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, NumSplats)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, SortValueList)
		SHADER_PARAMETER_SRV(StructuredBuffer<SplatView>, TheSplatViewData)
	END_SHADER_PARAMETER_STRUCT()
public:
	static bool ShouldCompilePermutation(const FShaderPermutationParameters& Params)
	{
		return true;
	}
};

class FOffscreenGaussianQuadFastPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOffscreenGaussianQuadFastPS)
    SHADER_USE_PARAMETER_STRUCT(FOffscreenGaussianQuadFastPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector4f, VecScreenParams)
	END_SHADER_PARAMETER_STRUCT()
public:
    static bool ShouldCompilePermutation(const FShaderPermutationParameters& Params)
    {
        return true;
    }

};


class FOffscreenGaussianQuadBakeDepthPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOffscreenGaussianQuadBakeDepthPS)
	SHADER_USE_PARAMETER_STRUCT(FOffscreenGaussianQuadBakeDepthPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, SplatColorMap)
		SHADER_PARAMETER(FVector2f, SplatColorMapUVScale)
		SHADER_PARAMETER(FVector4f, DepthOutputThreshold)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
	END_SHADER_PARAMETER_STRUCT()
public:
	static bool ShouldCompilePermutation(const FShaderPermutationParameters& Params)
	{
		return true;
	}

};


// Essentially same as above, don't know how to subclass from it make this simplier
class FOffscreenGaussianQuadBakeDepthFastPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOffscreenGaussianQuadBakeDepthFastPS)
	SHADER_USE_PARAMETER_STRUCT(FOffscreenGaussianQuadBakeDepthFastPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, SplatColorMap)
		SHADER_PARAMETER(FVector2f, SplatColorMapUVScale)
		SHADER_PARAMETER(FVector4f, DepthOutputThreshold)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
	END_SHADER_PARAMETER_STRUCT()
public:
	static bool ShouldCompilePermutation(const FShaderPermutationParameters& Params)
	{
		return true;
	}

};





class FOffscreenGaussianQuadAlphaCutoutVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FOffscreenGaussianQuadAlphaCutoutVS, Global)

	FOffscreenGaussianQuadAlphaCutoutVS() {}
	FOffscreenGaussianQuadAlphaCutoutVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}

	static bool ShouldCompilePermutation(const FShaderPermutationParameters& Params)
	{
		return true;
	}
};


class FOffscreenGaussianQuadAlphaCutoutPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOffscreenGaussianQuadAlphaCutoutPS)
	SHADER_USE_PARAMETER_STRUCT(FOffscreenGaussianQuadAlphaCutoutPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, SplatColorMap)
		SHADER_PARAMETER(FVector4f, DepthOutputThreshold)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
	END_SHADER_PARAMETER_STRUCT()
public:
	static bool ShouldCompilePermutation(const FShaderPermutationParameters& Params)
	{
		return true;
	}
};
