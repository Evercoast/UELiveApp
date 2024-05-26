#pragma once
#include "GlobalShader.h"
#include "RHIStaticStates.h"
#include "ShaderParameterUtils.h"


class FDrawQuadVS : public FGlobalShader
{
    DECLARE_EXPORTED_SHADER_TYPE(FDrawQuadVS, Global, EVERCOASTPLAYBACK_API);

    FDrawQuadVS() { }
    FDrawQuadVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FGlobalShader(Initializer)
    {
    }

    static bool ShouldCompilePermutation(const FShaderPermutationParameters& Params)
    {
        return true;
    }
};

class FDrawQuadGaussianBlurHorizontalPS : public FGlobalShader
{
    DECLARE_EXPORTED_SHADER_TYPE(FDrawQuadGaussianBlurHorizontalPS, Global, EVERCOASTPLAYBACK_API);

    LAYOUT_FIELD(FShaderResourceParameter, Filter_Texture);
    LAYOUT_FIELD(FShaderResourceParameter, Filter_Sampler);
    LAYOUT_FIELD(FShaderParameter, Filter_Texture_Size);

    FDrawQuadGaussianBlurHorizontalPS() { }
    FDrawQuadGaussianBlurHorizontalPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FGlobalShader(Initializer)
    {
        Filter_Texture_Size.Bind(Initializer.ParameterMap, TEXT("Filter_Texture_Size"), SPF_Mandatory);
        Filter_Texture.Bind(Initializer.ParameterMap, TEXT("Filter_Texture"), SPF_Mandatory);
        Filter_Sampler.Bind(Initializer.ParameterMap, TEXT("Filter_Sampler"), SPF_Mandatory);
    }
#if ENGINE_MAJOR_VERSION == 5
    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Params, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Params, OutEnvironment);
    }
#else
    static void ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Params, OutEnvironment);
    }
#endif
    static bool ShouldCompilePermutation(const FShaderPermutationParameters& Params)
    {
        return true;
    }
public:
    void SetFilterTexture(FRHICommandList& RHICmdList, const FTexture* Texture)
    {
        FVector2f Size(Texture->GetSizeX(), Texture->GetSizeY());
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
        FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();
        SetTextureParameter(BatchedParameters, Filter_Texture, Filter_Sampler, Texture);
        SetShaderValue(BatchedParameters, Filter_Texture_Size, Size);
        RHICmdList.SetBatchedShaderParameters(RHICmdList.GetBoundPixelShader(), BatchedParameters);
#else
        SetTextureParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), Filter_Texture, Filter_Sampler, Texture);
        SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(), Filter_Texture_Size, Size);
#endif
    }

    void SetFilterTexture(FRHICommandList& RHICmdList, const FTexture2DRHIRef Texture)
    {
        FVector2f Size(Texture->GetSizeX(), Texture->GetSizeY());
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
        FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();
        SetTextureParameter(BatchedParameters, Filter_Texture, Filter_Sampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), Texture);
        SetShaderValue(BatchedParameters, Filter_Texture_Size, Size);
        RHICmdList.SetBatchedShaderParameters(RHICmdList.GetBoundPixelShader(), BatchedParameters);
#else
        SetTextureParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), Filter_Texture, Filter_Sampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), Texture);
        SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(), Filter_Texture_Size, Size);
#endif
    }
};


class FDrawQuadGaussianBlurVerticalPS : public FGlobalShader
{
    DECLARE_EXPORTED_SHADER_TYPE(FDrawQuadGaussianBlurVerticalPS, Global, EVERCOASTPLAYBACK_API);

    LAYOUT_FIELD(FShaderResourceParameter, Filter_Texture);
    LAYOUT_FIELD(FShaderResourceParameter, Filter_Sampler);
    LAYOUT_FIELD(FShaderParameter, Filter_Texture_Size);

    FDrawQuadGaussianBlurVerticalPS() { }
    FDrawQuadGaussianBlurVerticalPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FGlobalShader(Initializer)
    {
        Filter_Texture_Size.Bind(Initializer.ParameterMap, TEXT("Filter_Texture_Size"), SPF_Mandatory);
        Filter_Texture.Bind(Initializer.ParameterMap, TEXT("Filter_Texture"), SPF_Mandatory);
        Filter_Sampler.Bind(Initializer.ParameterMap, TEXT("Filter_Sampler"), SPF_Mandatory);
    }

#if ENGINE_MAJOR_VERSION == 5
    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Params, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Params, OutEnvironment);
    }
#else
    static void ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Params, OutEnvironment);
    }
#endif

    static bool ShouldCompilePermutation(const FShaderPermutationParameters& Params)
    {
        return true;
    }
public:
    void SetFilterTexture(FRHICommandList& RHICmdList, const FTexture* Texture)
    {
        FVector2f Size(Texture->GetSizeX(), Texture->GetSizeY());
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3

        FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();
        SetTextureParameter(BatchedParameters, Filter_Texture, Filter_Sampler, Texture);
        SetShaderValue(BatchedParameters, Filter_Texture_Size, Size);
        RHICmdList.SetBatchedShaderParameters(RHICmdList.GetBoundPixelShader(), BatchedParameters);

#else
        SetTextureParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), Filter_Texture, Filter_Sampler, Texture);
        SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(), Filter_Texture_Size, Size);
#endif
    }

    void SetFilterTexture(FRHICommandList& RHICmdList, const FTexture2DRHIRef Texture)
    {
        FVector2f Size(Texture->GetSizeX(), Texture->GetSizeY());
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
        FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();
        SetTextureParameter(BatchedParameters, Filter_Texture, Filter_Sampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), Texture);
        SetShaderValue(BatchedParameters, Filter_Texture_Size, Size);
        RHICmdList.SetBatchedShaderParameters(RHICmdList.GetBoundPixelShader(), BatchedParameters);
#else
        SetTextureParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), Filter_Texture, Filter_Sampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), Texture);
        SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(), Filter_Texture_Size, Size);
#endif
    }
};


class FDrawQuadPS: public FGlobalShader
{
    DECLARE_EXPORTED_SHADER_TYPE(FDrawQuadPS, Global, EVERCOASTPLAYBACK_API);

    LAYOUT_FIELD(FShaderResourceParameter, Source_Texture);
    LAYOUT_FIELD(FShaderResourceParameter, Source_Sampler);

    FDrawQuadPS() { }
    FDrawQuadPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FGlobalShader(Initializer)
    {
        Source_Texture.Bind(Initializer.ParameterMap, TEXT("Filter_Texture"), SPF_Mandatory);
        Source_Sampler.Bind(Initializer.ParameterMap, TEXT("Filter_Sampler"), SPF_Mandatory);
    }

    static bool ShouldCompilePermutation(const FShaderPermutationParameters& Params)
    {
        // Could skip compiling for Platform == SP_METAL for example
        return true;
    }
public:
    void SetSourceTexture(FRHICommandList& RHICmdList, const FTexture* Texture)
    {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3

        FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();
        SetTextureParameter(BatchedParameters, Source_Texture, Source_Sampler, Texture);
        RHICmdList.SetBatchedShaderParameters(RHICmdList.GetBoundPixelShader(), BatchedParameters);

#else
        SetTextureParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), Source_Texture, Source_Sampler, Texture);
#endif
    }

    void SetSourceTexture(FRHICommandList& RHICmdList, const FTexture2DRHIRef Texture)
    {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
        FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();
        SetTextureParameter(BatchedParameters, Source_Texture, Source_Sampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), Texture);
        RHICmdList.SetBatchedShaderParameters(RHICmdList.GetBoundPixelShader(), BatchedParameters);
#else
        SetTextureParameter(RHICmdList, RHICmdList.GetBoundPixelShader(), Source_Texture, Source_Sampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), Texture);
#endif
    }
};
