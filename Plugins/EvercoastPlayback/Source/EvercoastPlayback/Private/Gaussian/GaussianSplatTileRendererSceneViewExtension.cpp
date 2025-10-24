#include "Gaussian/GaussianSplatTileRendererSceneViewExtension.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineModule.h"
#include "PixelShaderUtils.h"
#include "Gaussian/GaussianSplatTileRenderer.h"

#if PLATFORM_WINDOWS

IMPLEMENT_GLOBAL_SHADER(FGaussianSplatCompositeColourPixelShader, "/EvercoastShaders/GaussianSplatComposite.usf", "CompositeColourPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FGaussianSplatCompositeColourOverlayPixelShader, "/EvercoastShaders/GaussianSplatComposite.usf", "CompositeColourOverlayPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FGaussianSplatEngineDepthResolvePixelShader, "/EvercoastShaders/GaussianSplatComposite.usf", "ResolveDepthPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FGaussianSplatCompositeDepthPixelShader, "/EvercoastShaders/GaussianSplatComposite.usf", "CompositeDepthPS", SF_Pixel);


FGaussianSplatTileRendererSceneViewExtension::TRegisteredSplatImage::TRegisteredSplatImage(FTextureRHIRef InColour, FTextureRHIRef InDepth, const FVector2f& InUVScale, const FVector& WorldPos, bool bInToCompositeDepth, uint8 compositionStage, int frameCount) :
	Colour(InColour), Depth(InDepth), UVScale(InUVScale), WorldPosition(WorldPos), bToCompositeDepth(bInToCompositeDepth), CompositionStage(compositionStage), FrameCount(frameCount)
{
}

FGaussianSplatTileRendererSceneViewExtension::TRegisteredSplatImage::~TRegisteredSplatImage()
{
}


FGaussianSplatTileRendererSceneViewExtension::TRegisteredTileRenderer::TRegisteredTileRenderer(TSharedPtr<FGaussianSplatTileRenderer> InRenderer, const FVector& InWorldPos, bool bInToCompositeDepth, uint8 compositionStage, int InFrameCount) :
	TileRenderer(InRenderer),
	WorldPos(InWorldPos),
	bToCompositeDepth(bInToCompositeDepth),
	CompositionStage(compositionStage),
	FrameCount(InFrameCount)
{

}

FGaussianSplatTileRendererSceneViewExtension::TRegisteredTileRenderer::~TRegisteredTileRenderer()
{

}

namespace
{
	TAutoConsoleVariable<int32> CVarShaderOn(
		TEXT("r.EvercoastGaussianSplatTileRendererEnable"),
		1,
		TEXT("Enable FGaussianSplatTileRendererSceneViewExtension Colour Output\n")
		TEXT(" 0: OFF;")
		TEXT(" 1: ON."),
		ECVF_RenderThreadSafe);
}


FGaussianSplatTileRendererSceneViewExtension::FGaussianSplatTileRendererSceneViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister)
{
}

void FGaussianSplatTileRendererSceneViewExtension::Initialize()
{
	m_postOpaqueRenderHandle = GetRendererModule().RegisterPostOpaqueRenderDelegate(FPostOpaqueRenderDelegate::CreateRaw(this, &FGaussianSplatTileRendererSceneViewExtension::OnPostOpaqueRender));
	m_overlayRenderHandle = GetRendererModule().RegisterOverlayRenderDelegate(FPostOpaqueRenderDelegate::CreateRaw(this, &FGaussianSplatTileRendererSceneViewExtension::OnOverlayRender));
}

void FGaussianSplatTileRendererSceneViewExtension::Deinitialize()
{
	if (m_postOpaqueRenderHandle.IsValid())
	{
		GetRendererModule().RemovePostOpaqueRenderDelegate(m_postOpaqueRenderHandle);
	}
	if (m_overlayRenderHandle.IsValid())
	{
		GetRendererModule().RemoveOverlayRenderDelegate(m_overlayRenderHandle);
	}
}

void FGaussianSplatTileRendererSceneViewExtension::RegisterSplatImage(FTextureRHIRef rendererImage, FTextureRHIRef rendererDepthImage, const FVector2f& uvScale, const FVector& WorldPos, bool toCompositeDepth, uint8 compositionStage, int frameCount)
{
	std::lock_guard<std::recursive_mutex> guard(m_imageMutex);
	m_registeredSplatImageList.Add(
		MakeShared<TRegisteredSplatImage>(rendererImage, rendererDepthImage, uvScale, WorldPos, toCompositeDepth, compositionStage, frameCount)
	);
}


void FGaussianSplatTileRendererSceneViewExtension::ClearRegisteredSplatImages()
{
	std::lock_guard<std::recursive_mutex> guard(m_imageMutex);
	m_registeredSplatImageList.Empty();
}

void FGaussianSplatTileRendererSceneViewExtension::RegisterTileRenderer(TSharedPtr<FGaussianSplatTileRenderer> pTileRenderer, const FVector& WorldPos, bool toCompositeDepth, uint8 compositionStage, int frameCount)
{
	std::lock_guard<std::recursive_mutex> guard(m_tileRendererMutex);

	m_registeredTileRenderer.Add(
		MakeShared<TRegisteredTileRenderer>(pTileRenderer, WorldPos, toCompositeDepth, compositionStage, frameCount)
	);
}

void FGaussianSplatTileRendererSceneViewExtension::ClearRegisteredTileRenderer()
{
	std::lock_guard<std::recursive_mutex> guard(m_tileRendererMutex);
	m_registeredTileRenderer.Empty();
}

void FGaussianSplatTileRendererSceneViewExtension::PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& SceneView, const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{
	int renderMode = CVarShaderOn.GetValueOnRenderThread();
	if (renderMode == 0)
	{
		ClearRegisteredTileRenderer();
		ClearRegisteredSplatImages();
		return;
	}

	check(IsInRenderingThread());

	// In 5.4 5.5
	// Use this RHICmdList to call UGaussianSplatTileRenderer::RunPipeline_RenderThread() get the image result without delay
	{
		std::lock_guard<std::recursive_mutex> guard(m_tileRendererMutex);

		FRHICommandListImmediate& RHICmdList = GraphBuilder.RHICmdList;
		for (int i = 0; i < m_registeredTileRenderer.Num(); ++i)
		{
			TSharedPtr<TRegisteredTileRenderer> registeredTileRenderer = m_registeredTileRenderer[i];

			TSharedPtr<FGaussianSplatTileRenderer> tileRenderer = registeredTileRenderer->TileRenderer;

			// If the image was produced. Out of view or zero numbered splats will return false
			if (tileRenderer->RunPipelineWithLastSavedInput_RenderThread(RHICmdList))
			{

				// register image for code just below
				RegisterSplatImage(tileRenderer->GetOutputColourRenderTarget(), tileRenderer->GetOutputDepthRenderTarget(), tileRenderer->GetSavedOutputRenderTargetUVScale(), 
					registeredTileRenderer->WorldPos, registeredTileRenderer->bToCompositeDepth, registeredTileRenderer->CompositionStage, tileRenderer->GetOutputFrameCounter());
			}
		}

		ClearRegisteredTileRenderer();
	}

	

	// Inject depth here

	const FSceneViewFamily& ViewFamily = *SceneView.Family;
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(SceneView.GetFeatureLevel());

	FRDGTextureRef SceneDepthRDG = RenderTargets.DepthStencil.GetTexture();

	const FIntPoint sceneDepthSize = SceneDepthRDG->Desc.Extent;

	FIntRect ViewportUnscaled = SceneView.UnscaledViewRect;
	FIntRect ViewportRaw = SceneView.UnconstrainedViewRect;

	FScreenPassTexture SceneDepth((*SceneTextures)->SceneDepthTexture, FIntRect(0, 0, sceneDepthSize.X, sceneDepthSize.Y));

	// Desc for regular R32F to store SceneDepth in pass 1
	FRDGTextureDesc ResolvedDesc = FRDGTextureDesc::Create2D(
		sceneDepthSize, PF_R32_FLOAT, FClearValueBinding::None,
		TexCreate_ShaderResource | TexCreate_RenderTargetable); // RTV + SRV

	// This resolved depth texture should be reusable for multiple splats images
	FRDGTextureRef ResolvedDepthR32F = GraphBuilder.CreateTexture(ResolvedDesc, TEXT("ResolvedDepthR32F"));

	// Desc for dummy depth stencil in pass 1, copy most from the "official" depth stenil desc
	FRDGTextureDesc DummyDepthBufferDesc = SceneDepth.Texture->Desc;
	DummyDepthBufferDesc.Format = EPixelFormat::PF_DepthStencil;
	DummyDepthBufferDesc.ClearValue = FClearValueBinding(0);
	DummyDepthBufferDesc.Flags = TexCreate_DepthStencilTargetable | TexCreate_ShaderResource;
	DummyDepthBufferDesc.ClearValue = FClearValueBinding(0, 0);
	FRDGTextureRef DummyDepthRDGTexture = GraphBuilder.CreateTexture(DummyDepthBufferDesc, TEXT("ResolvedDepth DummyDepth"));


	// Desc for dummy colour target in pass 2
	FRDGTextureDesc DummyColourDesc = FRDGTextureDesc::Create2D(
		sceneDepthSize, PF_B8G8R8A8, FClearValueBinding::None,
		TexCreate_ShaderResource | TexCreate_RenderTargetable); // RTV + SRV

	// This dummy colour target should be reusable too
	FRDGTextureRef DummyTextureRGBA8 = GraphBuilder.CreateTexture(DummyColourDesc, TEXT("DummyColourWritable"));

	

	RDG_EVENT_SCOPE(GraphBuilder, "Gaussian Splats PostRenderBase Depth Composite Event");
	{
		std::lock_guard<std::recursive_mutex> guard(m_imageMutex);
		for (int i = 0; i < m_registeredSplatImageList.Num(); ++i)
		{
			TSharedPtr<TRegisteredSplatImage> registeredImage = m_registeredSplatImageList[i];
			if (!registeredImage->bToCompositeDepth) // If no depth inject(no Zwrite), then depth will only be used for Ztest
				continue;

			// -------------------- PASS 1: SceneDepth SRV -> R32F --------------------
			{
				FGaussianSplatEngineDepthResolvePixelShader::FParameters* P = GraphBuilder.AllocParameters<FGaussianSplatEngineDepthResolvePixelShader::FParameters>();
				P->SceneDepthSizeInvSize = FVector4f(sceneDepthSize.X, sceneDepthSize.Y, 1.0f / sceneDepthSize.X, 1.0f / sceneDepthSize.Y);
				P->SceneDepthTexture = GraphBuilder.CreateSRV(SceneDepthRDG);
				P->LinearClamp = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				// Write target is R32F RTV
				P->RenderTargets[0] = FRenderTargetBinding(ResolvedDepthR32F, ERenderTargetLoadAction::ENoAction);

				// Need a dummy depth stencil here
				FDepthStencilBinding DepthStencilBinding(
					DummyDepthRDGTexture,
					ERenderTargetLoadAction::ENoAction,
					ERenderTargetLoadAction::ENoAction,
					FExclusiveDepthStencil::DepthWrite_StencilWrite
				);
				P->RenderTargets.DepthStencil = DepthStencilBinding;

				// Fullscreen draw (pixel shader outputs float to RTV)
				TShaderMapRef<FGaussianSplatEngineDepthResolvePixelShader> PS(GlobalShaderMap);
				FPixelShaderUtils::AddFullscreenPass(
					GraphBuilder,
					GlobalShaderMap,
					RDG_EVENT_NAME("Resolve SceneDepth To R32F Pass"),
					PS,
					P,
					FIntRect(0, 0, sceneDepthSize.X, sceneDepthSize.Y),
					TStaticBlendState<>::GetRHI(),
					TStaticRasterizerState<>::GetRHI(),
					// Zwrite = false, Ztest = always
					TStaticDepthStencilState<false, CF_Always>::GetRHI()
				);
			}

			// ------------------ PASS 2: R32F Depth + Splat Depth -> SceneDepth ----------------------
			{
				FRHITexture* inputDepthImageRHITexture = registeredImage->Depth;

				FGaussianSplatCompositeDepthPixelShader::FParameters* Params = GraphBuilder.AllocParameters<FGaussianSplatCompositeDepthPixelShader::FParameters>();
				// Need a dummy colour here
				Params->RenderTargets[0] = FRenderTargetBinding(DummyTextureRGBA8, ERenderTargetLoadAction::ENoAction);
				Params->SceneDepthSizeInvSize = FVector4f(sceneDepthSize.X, sceneDepthSize.Y, 1.0f / sceneDepthSize.X, 1.0f / sceneDepthSize.Y);
				Params->ViewportSizeInvSize = FVector4f(ViewportUnscaled.Width(), ViewportUnscaled.Height(), 1.0f / ViewportUnscaled.Width(), 1.0f / ViewportUnscaled.Height());
				Params->SplatDepthSizeInvSize = FVector4f(inputDepthImageRHITexture->GetSizeX(), inputDepthImageRHITexture->GetSizeY(), 1.0f / inputDepthImageRHITexture->GetSizeX(), 1.0f / inputDepthImageRHITexture->GetSizeY());

				FIntRect realViewportRect = m_lastPostOpaqueViewportRect;
				if (realViewportRect.Width() == 0 || realViewportRect.Height() == 0)
				{
					// just use the full screen but incorrect for one frame
					realViewportRect = FIntRect(0, 0, sceneDepthSize.X, sceneDepthSize.Y);
				}
				// ASSUME COLOR / DEPTH ARE THE SAME.. IT MIGHT CHANGE?
				Params->SceneColorDepthUVScale = FVector2f(float(realViewportRect.Width()) / float(sceneDepthSize.X), float(realViewportRect.Height()) / float(sceneDepthSize.Y));

				/*
				UE_LOG(LogTemp, Log, TEXT("ViewportUnscaled: (%dx%d) PostOpaqueViewport: (%dx%d) SceneDepth: (%dx%d) SplatDepthRT: (%dx%d) SceneDepth Actual Render Area/Viewport Area Ratio: (%.2f, %.2f)"), 
					ViewportUnscaled.Width(), ViewportUnscaled.Height(), 
					m_lastPostOpaqueViewportRect.Width(), m_lastPostOpaqueViewportRect.Height(),
					sceneDepthSize.X, sceneDepthSize.Y,
					inputDepthImageRHITexture->GetSizeX(), inputDepthImageRHITexture->GetSizeY(),
					Params->SceneDepthUVScale.X, Params->SceneDepthUVScale.Y
				);
				*/

				Params->SplatUVScale = registeredImage->UVScale;
				Params->SplatDepthTexture = inputDepthImageRHITexture; // input 1
				Params->SceneDepthTextureR32F = ResolvedDepthR32F; // input 2
				Params->LinearClamp = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

				// Need to put "official" depth stencil back here
				FDepthStencilBinding DepthStencilBinding(
					SceneDepth.Texture,
					ERenderTargetLoadAction::ELoad,
					ERenderTargetLoadAction::ELoad,
					FExclusiveDepthStencil::DepthWrite_StencilWrite
				);
				Params->RenderTargets.DepthStencil = DepthStencilBinding;

				TShaderMapRef<FGaussianSplatCompositeDepthPixelShader> PS(GlobalShaderMap);
				FPixelShaderUtils::AddFullscreenPass(
					GraphBuilder,
					GlobalShaderMap,
					RDG_EVENT_NAME("Gaussian Splats Depth Composition Pass"),
					PS,
					Params,
					FIntRect(0, 0, sceneDepthSize.X, sceneDepthSize.Y),
					TStaticBlendState<>::GetRHI(),
					TStaticRasterizerState<>::GetRHI(),
					// ZWrite=true, ZTest=Always
					TStaticDepthStencilState <true, CF_Always>::GetRHI()
				);
			}
		}

		// Wait until colour composition to do clear registered image
	}

	
}



void FGaussianSplatTileRendererSceneViewExtension::OnPostOpaqueRender(FPostOpaqueRenderParameters& Parameters)
{
	int renderMode = CVarShaderOn.GetValueOnRenderThread();
	if (renderMode == 0)
	{
		ClearRegisteredSplatImages();
		return;
	}

	check(IsInRenderingThread());

	m_lastPostOpaqueViewportRect = Parameters.ViewportRect;

	const FViewInfo* ViewInfo = Parameters.View;

	FRDGBuilder& GraphBuilder = *Parameters.GraphBuilder;

	

	const ERHIFeatureLevel::Type FeatureLevel = ViewInfo->FeatureLevel;

	RDG_EVENT_SCOPE(GraphBuilder, "Gaussian Splats PostOpaque Colour Composite Event");
	{
		// First sort the registered array by view Z to slightly improve "splats over splats" occlusion
		const FMatrix& ViewMatrix = Parameters.ViewMatrix;

		// Further away along the Z axis will be drawn first (painter's algorithm)
		m_registeredSplatImageList.StableSort([ViewMatrix](const TSharedPtr<TRegisteredSplatImage>& img1, const TSharedPtr<TRegisteredSplatImage>& img2)
			{
				FVector4 pos1 = ViewMatrix.TransformPosition(img1->WorldPosition);
				FVector4 pos2 = ViewMatrix.TransformPosition(img2->WorldPosition);

				return pos1.Z > pos2.Z;
			});

		// Accesspoint to our Shaders
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);

		std::lock_guard<std::recursive_mutex> guard(m_imageMutex);
		// For all registered images
		for (int i = 0; i < m_registeredSplatImageList.Num(); ++i)
		{
			TSharedPtr<TRegisteredSplatImage> registeredImage = m_registeredSplatImageList[i];
			if (registeredImage->CompositionStage != 0) // skip the one that's not registered at post opaque stage
				continue;

			FRHITexture* inputImageRHITexture = registeredImage->Colour;
			FRHITexture* inputDepthImageRHITexture = registeredImage->Depth;

			// Setup all the descriptors to create a target texture
			FRDGTextureDesc OutputDesc;
			{
				OutputDesc = Parameters.ColorTexture->Desc;
				OutputDesc.Reset();
				// Make rendertargetable
				OutputDesc.Flags |= TexCreate_RenderTargetable;

				// DEBUG READ
				FLinearColor ClearColor(1., 0., 0., 1.);
				OutputDesc.ClearValue = FClearValueBinding(ClearColor);
			}

			

			// Set the shader parameters
			FGaussianSplatCompositeColourPixelShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FGaussianSplatCompositeColourPixelShader::FParameters>();

			// Have to set this ViewUnifomBuffer?
			FCommonShaderParameters CommonParameters;
			CommonParameters.ViewUniformBuffer = ViewInfo->ViewUniformBuffer;
			PassParameters->CommonParameters = CommonParameters;

			/////////// INPUT ////////////////
			// Input is the SceneColor from PostProcess Material Inputs
			PassParameters->OriginalSceneColor = Parameters.ColorTexture;
			PassParameters->OriginalSceneDepth = Parameters.DepthTexture;

			// This binding is import to access scene textures like depth and using utility function LookupDeviceZ() etc
			PassParameters->SceneTextures = Parameters.SceneTexturesUniformParams;

			PassParameters->SplatInputColor = inputImageRHITexture;
			PassParameters->SplatInputColorDepthSizeInvSize = FVector4f(inputImageRHITexture->GetSizeX(), inputImageRHITexture->GetSizeY(),
				1.0f / inputImageRHITexture->GetSizeX(), 1.0f / inputImageRHITexture->GetSizeY());

			PassParameters->SplatInputDepth = inputDepthImageRHITexture;

			// Get the input sizes (do note that viewport visible area might not be the full extent of the SceneColor texture
			// https://docs.unrealengine.com/5.1/en-US/screen-percentage-with-temporal-upscale-in-unreal-engine/
			FIntRect ViewportRect = Parameters.ViewportRect;
			FIntPoint ColorTextureSize = Parameters.ColorTexture->Desc.Extent;

			PassParameters->ViewportSizeInvSize = FVector4f(ViewportRect.Width(), ViewportRect.Height(), 1.0f / ViewportRect.Width(), 1.0f / ViewportRect.Height());

//			UE_LOG(LogTemp, Log, TEXT("Viewport: (%dx%d) Splat RT: (%dx%d)"), ViewportRect.Max.X, ViewportRect.Max.Y, inputImageRHITexture->GetSizeX(), inputImageRHITexture->GetSizeY());

			// Conversion from the full texture to the actual used size
			// Refer to Screenpass.h to see how UE handles scaling of the different viewport sizes

			// ASSUME DEPTH UV SCALE IS THE SAME...
			PassParameters->SceneColorDepthUVScale = FVector2f(float(ViewportRect.Width()) / float(ColorTextureSize.X), float(ViewportRect.Height()) / float(ColorTextureSize.Y));

			PassParameters->SplatUVScale = registeredImage->UVScale;

			// Official depth map dimension
			FIntPoint sceneDepthSize = Parameters.DepthTexture->Desc.Extent;
			PassParameters->SceneDepthSizeInvSize = FVector4f(sceneDepthSize.X, sceneDepthSize.Y, 1.0f / sceneDepthSize.X, 1.0f / sceneDepthSize.Y);

			PassParameters->LinearClamp = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

			/////////// OUTPUT /////////////
			// Create colour target texture
			FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("Gaussian Splats Colour Composition Output Texture"));
			PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ENoAction);

			// Desc for dummy depth stencil in pass 1, copy most from the "official" depth stenil desc
			FRDGTextureDesc DummyDepthBufferDesc = Parameters.DepthTexture->Desc;
			DummyDepthBufferDesc.Format = EPixelFormat::PF_DepthStencil;
			DummyDepthBufferDesc.ClearValue = FClearValueBinding(0);
			DummyDepthBufferDesc.Flags = TexCreate_DepthStencilTargetable | TexCreate_ShaderResource;
			DummyDepthBufferDesc.ClearValue = FClearValueBinding(0, 0);
			FRDGTextureRef DummyDepthRDGTexture = GraphBuilder.CreateTexture(DummyDepthBufferDesc, TEXT("CopyColour DummyDepth"));

			// Need a dummy depth stencil here
			FDepthStencilBinding DepthStencilBinding(
				DummyDepthRDGTexture,
				ERenderTargetLoadAction::ENoAction,
				ERenderTargetLoadAction::ENoAction,
				FExclusiveDepthStencil::DepthWrite_StencilWrite
			);
			PassParameters->RenderTargets.DepthStencil = DepthStencilBinding;

			TShaderMapRef<FGaussianSplatCompositeColourPixelShader> PixelShader(GlobalShaderMap);
			// Pass 1: Compute shader pass to blend splat colour into scene colour, Ztest ON
			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder,
				GlobalShaderMap,
				RDG_EVENT_NAME("Gaussian Splats Colour Composition Pass"),
				PixelShader,
				PassParameters,
				Parameters.ViewportRect,
				TStaticBlendState<>::GetRHI(),
				TStaticRasterizerState<>::GetRHI(),
				// ZWrite=false, ZTest=Always
				TStaticDepthStencilState <false, CF_Always>::GetRHI()
			);

			// Pass 2: Copy the output texture back to "official" SceneColor
			AddCopyTexturePass(GraphBuilder, OutputTexture, Parameters.ColorTexture);
		}

		// Wait until overlay render to do clear registered image

	}
}

void FGaussianSplatTileRendererSceneViewExtension::OnOverlayRender(FPostOpaqueRenderParameters& Parameters)
{
	int renderMode = CVarShaderOn.GetValueOnRenderThread();
	if (renderMode == 0)
	{
		ClearRegisteredSplatImages();
		return;
	}

	m_lastPostOpaqueViewportRect = Parameters.ViewportRect;

	const FViewInfo* ViewInfo = Parameters.View;

	FRDGBuilder& GraphBuilder = *Parameters.GraphBuilder;
	const ERHIFeatureLevel::Type FeatureLevel = ViewInfo->FeatureLevel;

	RDG_EVENT_SCOPE(GraphBuilder, "Gaussian Splats Overlay Colour Composite Event");
	{
		// First sort the registered array by view Z to slightly improve "splats over splats" occlusion
		const FMatrix& ViewMatrix = Parameters.ViewMatrix;

		// Further away along the Z axis will be drawn first (painter's algorithm)
		m_registeredSplatImageList.StableSort([ViewMatrix](const TSharedPtr<TRegisteredSplatImage>& img1, const TSharedPtr<TRegisteredSplatImage>& img2)
			{
				FVector4 pos1 = ViewMatrix.TransformPosition(img1->WorldPosition);
				FVector4 pos2 = ViewMatrix.TransformPosition(img2->WorldPosition);

				return pos1.Z > pos2.Z;
			});

		// Accesspoint to our Shaders
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);

		std::lock_guard<std::recursive_mutex> guard(m_imageMutex);
		// For all registered images
		for (int i = 0; i < m_registeredSplatImageList.Num(); ++i)
		{
			TSharedPtr<TRegisteredSplatImage> registeredImage = m_registeredSplatImageList[i];
			if (registeredImage->CompositionStage != 1) // skip the one that's not registered at overlay stage
				continue;

			FRHITexture* inputImageRHITexture = registeredImage->Colour;
			FRHITexture* inputDepthImageRHITexture = registeredImage->Depth;

			// Setup all the descriptors to create a target texture
			FRDGTextureDesc OutputDesc;
			{
				OutputDesc = Parameters.ColorTexture->Desc;
				OutputDesc.Reset();
				// Make rendertargetable
				OutputDesc.Flags |= TexCreate_RenderTargetable;

				// DEBUG READ
				FLinearColor ClearColor(1., 0., 0., 1.);
				OutputDesc.ClearValue = FClearValueBinding(ClearColor);
			}



			// Set the shader parameters
			FGaussianSplatCompositeColourOverlayPixelShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FGaussianSplatCompositeColourOverlayPixelShader::FParameters>();


			/////////// INPUT ////////////////
			// Input is the SceneColor from PostProcess Material Inputs
			PassParameters->OriginalSceneColor = Parameters.ColorTexture;
			PassParameters->OriginalSceneDepth = Parameters.DepthTexture;


			PassParameters->SplatInputColor = inputImageRHITexture;
			PassParameters->SplatInputColorDepthSizeInvSize = FVector4f(inputImageRHITexture->GetSizeX(), inputImageRHITexture->GetSizeY(),
				1.0f / inputImageRHITexture->GetSizeX(), 1.0f / inputImageRHITexture->GetSizeY());

			PassParameters->SplatInputDepth = inputDepthImageRHITexture;

			// Get the input sizes (do note that viewport visible area might not be the full extent of the SceneColor texture
			// https://docs.unrealengine.com/5.1/en-US/screen-percentage-with-temporal-upscale-in-unreal-engine/
			FIntRect ViewportRect = Parameters.ViewportRect;
			FIntPoint ColorTextureSize = Parameters.ColorTexture->Desc.Extent;

			PassParameters->ViewportSizeInvSize = FVector4f(ViewportRect.Width(), ViewportRect.Height(), 1.0f / ViewportRect.Width(), 1.0f / ViewportRect.Height());

			//			UE_LOG(LogTemp, Log, TEXT("Viewport: (%dx%d) Splat RT: (%dx%d)"), ViewportRect.Max.X, ViewportRect.Max.Y, inputImageRHITexture->GetSizeX(), inputImageRHITexture->GetSizeY());

						// Conversion from the full texture to the actual used size
						// Refer to Screenpass.h to see how UE handles scaling of the different viewport sizes

						// ASSUME DEPTH UV SCALE IS THE SAME...
			PassParameters->SceneColorDepthUVScale = FVector2f(float(ViewportRect.Width()) / float(ColorTextureSize.X), float(ViewportRect.Height()) / float(ColorTextureSize.Y));

			PassParameters->SplatUVScale = registeredImage->UVScale;

			// Official depth map dimension
			FIntPoint sceneDepthSize = Parameters.DepthTexture->Desc.Extent;
			PassParameters->SceneDepthSizeInvSize = FVector4f(sceneDepthSize.X, sceneDepthSize.Y, 1.0f / sceneDepthSize.X, 1.0f / sceneDepthSize.Y);

			PassParameters->LinearClamp = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

			/////////// OUTPUT /////////////
			// Create colour target texture
			FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("Gaussian Splats Colour Composition Output Texture"));
			PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ENoAction);

			// Desc for dummy depth stencil in pass 1, copy most from the "official" depth stenil desc
			FRDGTextureDesc DummyDepthBufferDesc = Parameters.DepthTexture->Desc;
			DummyDepthBufferDesc.Format = EPixelFormat::PF_DepthStencil;
			DummyDepthBufferDesc.ClearValue = FClearValueBinding(0);
			DummyDepthBufferDesc.Flags = TexCreate_DepthStencilTargetable | TexCreate_ShaderResource;
			DummyDepthBufferDesc.ClearValue = FClearValueBinding(0, 0);
			FRDGTextureRef DummyDepthRDGTexture = GraphBuilder.CreateTexture(DummyDepthBufferDesc, TEXT("CopyColour DummyDepth"));

			// Need a dummy depth stencil here
			FDepthStencilBinding DepthStencilBinding(
				DummyDepthRDGTexture,
				ERenderTargetLoadAction::ENoAction,
				ERenderTargetLoadAction::ENoAction,
				FExclusiveDepthStencil::DepthWrite_StencilWrite
			);
			PassParameters->RenderTargets.DepthStencil = DepthStencilBinding;

			TShaderMapRef<FGaussianSplatCompositeColourOverlayPixelShader> PixelShader(GlobalShaderMap);
			// Pass 1: Compute shader pass to blend splat colour into scene colour, Ztest ON
			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder,
				GlobalShaderMap,
				RDG_EVENT_NAME("Gaussian Splats Colour Composition Pass"),
				PixelShader,
				PassParameters,
				Parameters.ViewportRect,
				TStaticBlendState<>::GetRHI(),
				TStaticRasterizerState<>::GetRHI(),
				// ZWrite=false, ZTest=Always
				TStaticDepthStencilState <false, CF_Always>::GetRHI()
			);

			// Pass 2: Copy the output texture back to "official" SceneColor
			AddCopyTexturePass(GraphBuilder, OutputTexture, Parameters.ColorTexture);
		}

		// This is the final chance to clean up registered splat image for this frame
		ClearRegisteredSplatImages();

	}
}
#endif