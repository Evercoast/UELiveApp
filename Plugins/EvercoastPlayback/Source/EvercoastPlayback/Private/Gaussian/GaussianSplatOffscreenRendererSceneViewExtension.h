#pragma once
#include "CoreMinimal.h"
#include "RenderGraphUtils.h"
#include "SceneViewExtension.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessMaterial.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "DataDrivenShaderPlatformInfo.h"
#endif
#include <mutex>



class FRDGBuilder;
class UTextureRenderTarget2D;
class FGaussianSplatTileRenderer;
class FGaussianSplatOffscreenQuadRenderer;

class FPostOpaqueRenderParameters;

// This scene view extension has two jobs to do:
// 1. Inject splats depth into the existing depth map, when base pass just finishes
// 2. Merge splats colour into the existing colour map, when all opaque objects finish rendering
// This is done through both FSceneViewExtensionBase and FPostOpaqueRenderDelegate interfaces
class FGaussianSplatOffscreenRendererSceneViewExtension : public FSceneViewExtensionBase
{
public:
	FGaussianSplatOffscreenRendererSceneViewExtension(const FAutoRegister& AutoRegister);

public:
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& InView, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;
#else
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass PassId, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;
#endif
	// If we need Gaussians to occlude other Gaussians properly, then we'll have to inject our depth in depth prepass, so that camera view can have an aligned depth information to test with
	// Using quad-renderer's depth will introduce lots of artefacts and it won't work well with transparent-quad based quad renderer
	// PostRenderBasePassDeferred_RenderThread is called right after Base Pass. This the place we blend in splat's depth
	virtual void PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView, const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) override;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3	
	virtual void PostRenderBasePassMobile_RenderThread(FRHICommandList& RHICmdList, FSceneView& InView) override;
#else
	virtual void PostRenderBasePassMobile_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;
#endif

#if PLATFORM_WINDOWS
	// Called by external code after the tile renderer has recorded its pass information(by calling SaveInput). The reference of the tile renderer will be only kept for this frame and be removed by ClearRegisteredTileRenderer()
	// The registered renderer will be used to produce the colour and depth image at a proper stage(at PostRenderBasePassDeferred_RenderThread)
	void RegisterTileRenderer(TSharedPtr<FGaussianSplatTileRenderer> pTileRenderer, const FVector& WorldPos, bool toCompositeDepth, uint8 compositionStage);
#endif
	// Same as RegisterTileRenderer but for offscreen quda renderer
	void RegisterOffscreenQuadRenderer(TSharedPtr<FGaussianSplatOffscreenQuadRenderer> pQuadRenderer, const FVector& WorldPos, bool toCompositeDepth, uint8 compositionStage);

	// Be explicit
	void Initialize();
	void Deinitialize();

private:

	struct TRegisteredSplatImage
	{
		FTextureRHIRef Colour;
		FTextureRHIRef Depth;
		FVector2f UVScale;
		FVector WorldPosition;
		bool bToCompositeDepth;
		uint8 CompositionStage;

		TRegisteredSplatImage(FTextureRHIRef InColour, FTextureRHIRef InDepth, const FVector2f& InUVScale, const FVector& WorldPos, bool bInToCompositeDepth, uint8 compositionStage);
		~TRegisteredSplatImage();
	};
#if PLATFORM_WINDOWS
	struct TRegisteredTileRenderer
	{
		TSharedPtr<FGaussianSplatTileRenderer> TileRenderer;
		FVector WorldPos;
		bool bToCompositeDepth;
		uint8 CompositionStage;

		TRegisteredTileRenderer(TSharedPtr<FGaussianSplatTileRenderer> InRenderer, const FVector& InWorldPos, bool bInToCompositeDepth, uint8 compositionStage);
		~TRegisteredTileRenderer();
	};
#endif
	struct TRegisteredOffscreenQuadRenderer
	{
		TSharedPtr<FGaussianSplatOffscreenQuadRenderer> QuadRenderer;
		FVector WorldPos;
		bool bToCompositeDepth;
		uint8 CompositionStage;

		TRegisteredOffscreenQuadRenderer(TSharedPtr<FGaussianSplatOffscreenQuadRenderer> InRenderer, const FVector& InWorldPos, bool bInToCompositeDepth, uint8 compositionStage);
		~TRegisteredOffscreenQuadRenderer();
	};

	// Called by each tile renderer/offscreen quad renderer to register its final image to be composited to Unreal scene
	// Optionally to composite/inject the splat's depth, but it will introduce artefacts (depending on how the depth image was generated, fine tuneable at the renderer component)
	// Can be called on both main thread and render thread
	void RegisterSplatImage(FTextureRHIRef rendererColourImage, FTextureRHIRef rendererDepthImage, const FVector2f& uvScale, const FVector& WorldPos, bool toCompositeDepth, uint8 compositionStage);

	// To blend in colour image, we have to use FPostOpaqueRenderDelegate interface. This will be called just after all opaque objects are rendered(before translucency, post process)
	
	// Two places to blend colour: Post Opaque will work with rest of the scene. 
	// Overlay is suitable for high resolution image output
	void OnPostOpaqueRender(FPostOpaqueRenderParameters& params);
	void OnOverlayRender(FPostOpaqueRenderParameters& params);

	// Called eveytime when composite finished, and when colour/depth image change sizes/resource reinit
	// Can be called on both main thread and render thread
	void ClearRegisteredSplatImages();

	// Remove the reference of registered renderers
	// These will be called once the renderer has produced the colour/depth image
#if PLATFORM_WINDOWS
	void ClearRegisteredTileRenderer();
#endif
	void ClearRegisteredOffscreenQuadRenderer();

	FScreenPassTexture OnPostProcessingTonemap(FRDGBuilder& GraphBuilder, const FSceneView& SceneView, const FPostProcessMaterialInputs& Inputs);



	std::recursive_mutex m_imageMutex;
	TArray<TSharedPtr<TRegisteredSplatImage>> m_registeredSplatImageList;
#if PLATFORM_WINDOWS
	std::recursive_mutex m_tileRendererMutex;
	TArray<TSharedPtr<TRegisteredTileRenderer>> m_registeredTileRenderer;
#endif
	std::recursive_mutex m_offscreenQuadRendererMutex;
	TArray<TSharedPtr<TRegisteredOffscreenQuadRenderer>> m_registeredOffscreenQuadRenderer;

	FDelegateHandle m_postOpaqueRenderHandle;
	FDelegateHandle m_overlayRenderHandle;
	FDelegateHandle m_resolvedSceneColorRenderHandle;

};



// Shader declarations

// Struct to include common parameters, useful when doing multiple shaders
BEGIN_SHADER_PARAMETER_STRUCT(FCommonShaderParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
END_SHADER_PARAMETER_STRUCT()


// Unreal "Official" Depth Buffer Resolve Fullscreen Shader
// Copy the special PF_DepthStencil buffer out to a regular R32F one
// Note before doing that, we'll need to setup a dummy PF_DepthStencil buffer because you cannot read from 
// one buffer and render target to the same one. And so that RDG won't optimize the pass out.
class FGaussianSplatEngineDepthResolvePixelShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGaussianSplatEngineDepthResolvePixelShader)
	SHADER_USE_PARAMETER_STRUCT(FGaussianSplatEngineDepthResolvePixelShader, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector4f, SceneDepthSizeInvSize)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, SceneDepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		//return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
		return true;
	}
};

// Gaussian Splats Fullscreen Depth Composition Shader
// Uses the output from FGaussianSplatEngineDepthResolvePixelShader to do Z-test and write back to the "official" PF_DepthStencil one
// Note before doing that, don't forget to set the "official" PF_DepthStencil back to the render target slot
class FGaussianSplatCompositeDepthPixelShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGaussianSplatCompositeDepthPixelShader)
	SHADER_USE_PARAMETER_STRUCT(FGaussianSplatCompositeDepthPixelShader, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector4f, ViewportSizeInvSize)
		SHADER_PARAMETER(FVector4f, SceneDepthSizeInvSize)
		SHADER_PARAMETER(FVector4f, SplatDepthSizeInvSize)
		SHADER_PARAMETER(FVector2f, SceneColorDepthUVScale)
		SHADER_PARAMETER(FVector2f, SplatUVScale)
		SHADER_PARAMETER_TEXTURE(Texture2D<float>, SplatDepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, SceneDepthTextureR32F)
		RENDER_TARGET_BINDING_SLOTS()                                                
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		//return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
		return true;
	}

};


class FGaussianSplatEngineGBufferModifierPixelShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGaussianSplatEngineGBufferModifierPixelShader)
	SHADER_USE_PARAMETER_STRUCT(FGaussianSplatEngineGBufferModifierPixelShader, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, SplatInputColor)
		SHADER_PARAMETER_TEXTURE(Texture2D, SplatInputDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OriginalGBuffer)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OriginalSceneDepth)
		SHADER_PARAMETER(FVector4f, ViewportSizeInvSize)
		SHADER_PARAMETER(FVector2f, SceneColorDepthUVScale)
		SHADER_PARAMETER(FVector2f, SplatUVScale)

		SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		//return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
		return true;
	}
};

// Gaussian Splat Fullscreen Colour Composition Shader
class FGaussianSplatCompositeColourPixelShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGaussianSplatCompositeColourPixelShader)
	SHADER_USE_PARAMETER_STRUCT(FGaussianSplatCompositeColourPixelShader, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FCommonShaderParameters, CommonParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
		SHADER_PARAMETER(FVector4f, ViewportSizeInvSize)
		SHADER_PARAMETER(FVector2f, SceneColorDepthUVScale)
		SHADER_PARAMETER(FVector2f, SplatUVScale)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OriginalSceneColor)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OriginalSceneDepth)
		SHADER_PARAMETER_TEXTURE(Texture2D, SplatInputColor)
		SHADER_PARAMETER_TEXTURE(Texture2D, SplatInputDepth)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	// Basic shader stuff
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		//return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
		return true;
	}
};


class FGaussianSplatCompositeColourOverlayPixelShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGaussianSplatCompositeColourOverlayPixelShader)
	SHADER_USE_PARAMETER_STRUCT(FGaussianSplatCompositeColourOverlayPixelShader, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector4f, ViewportSizeInvSize)
		SHADER_PARAMETER(FVector2f, SceneColorDepthUVScale)
		SHADER_PARAMETER(FVector2f, SplatUVScale)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OriginalSceneColor)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OriginalSceneDepth)
		SHADER_PARAMETER_TEXTURE(Texture2D, SplatInputColor)
		SHADER_PARAMETER_TEXTURE(Texture2D, SplatInputDepth)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	// Basic shader stuff
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		//return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
		return true;
	}
};

class FGaussianSplatCompositeColourPostToneMappingPixelShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGaussianSplatCompositeColourPostToneMappingPixelShader)
	SHADER_USE_PARAMETER_STRUCT(FGaussianSplatCompositeColourPostToneMappingPixelShader, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FCommonShaderParameters, CommonParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
		SHADER_PARAMETER(FVector4f, ViewportSizeInvSize)
		SHADER_PARAMETER(FVector2f, SceneColorDepthUVScale)
		SHADER_PARAMETER(FVector2f, SplatUVScale)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OriginalSceneColor)
		SHADER_PARAMETER_TEXTURE(Texture2D, SplatInputColor)
		SHADER_PARAMETER_TEXTURE(Texture2D, SplatInputDepth)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	// Basic shader stuff
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		//return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
		return true;
	}
};
