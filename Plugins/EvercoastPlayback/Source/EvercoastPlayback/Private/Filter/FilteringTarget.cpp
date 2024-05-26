#include "FilteringTarget.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CommonRenderResources.h"
#include "SmoothShaders.h"
#include "NormalGenerationShaders.h"

// This needs to go on a cpp file
IMPLEMENT_SHADER_TYPE(, FDrawQuadVS, TEXT("/EvercoastShaders/Smooth.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FDrawQuadGaussianBlurHorizontalPS, TEXT("/EvercoastShaders/Smooth.usf"), TEXT("MainPS_Horizontal"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FDrawQuadGaussianBlurVerticalPS, TEXT("/EvercoastShaders/Smooth.usf"), TEXT("MainPS_Vertical"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FDrawQuadPS, TEXT("/EvercoastShaders/Smooth.usf"), TEXT("MainPS_CopyThrough"), SF_Pixel);

IMPLEMENT_SHADER_TYPE(, FWorldNormalGenVS, TEXT("/EvercoastShaders/WorldNormalGen.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FWorldNormalGenPS, TEXT("/EvercoastShaders/WorldNormalGen.usf"), TEXT("MainPS"), SF_Pixel);

IMPLEMENT_SHADER_TYPE(, FVoxelWorldNormalGenVS, TEXT("/EvercoastShaders/WorldNormalGen.usf"), TEXT("MainVoxelVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FVoxelWorldNormalGenPS, TEXT("/EvercoastShaders/WorldNormalGen.usf"), TEXT("MainVoxelPS"), SF_Pixel);


void FFlipFilterRenderTarget::DoFilter_Horizontal(const FTexture2DRHIRef& SourceTextureRHI, const FTexture2DRHIRef& DestinationTextureRHI, FRHICommandListImmediate& RHICmdList, IRendererModule* RendererModule)
{
	RHICmdList.Transition(FRHITransitionInfo(SourceTextureRHI, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(DestinationTextureRHI, ERHIAccess::Unknown, ERHIAccess::RTV));

	FRHIRenderPassInfo RPInfo(DestinationTextureRHI, ERenderTargetActions::Load_Store);

	RHICmdList.BeginRenderPass(RPInfo, TEXT("Horizontal Filtering"));

	{
		RHICmdList.SetViewport(0, 0, 0.0f, DestinationTextureRHI->GetSizeX(), DestinationTextureRHI->GetSizeY(), 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		// New engine version...
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FDrawQuadVS> VertexShader(ShaderMap);
		TShaderMapRef<FDrawQuadGaussianBlurHorizontalPS> PixelShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
#if ENGINE_MAJOR_VERSION >= 5
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
#else
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
#endif
		PixelShader->SetFilterTexture(RHICmdList, SourceTextureRHI);

		RendererModule->DrawRectangle(RHICmdList,
			0, 0,				             // Dest X, Y
			DestinationTextureRHI->GetSizeX(),  // Dest Width
			DestinationTextureRHI->GetSizeY(),  // Dest Height
			0, 0,                            // Source U, V
			1, 1,                            // Source USize, VSize
			DestinationTextureRHI->GetSizeXY(), // Target buffer size
			FIntPoint(1, 1),                 // Source texture size
			VertexShader, EDRF_Default);
	}

	RHICmdList.EndRenderPass();

}

void FFlipFilterRenderTarget::DoFilter_Vertical(const FTexture2DRHIRef& SourceTextureRHI, const FTexture2DRHIRef& DestinationTextureRHI, FRHICommandListImmediate& RHICmdList, IRendererModule* RendererModule)
{
	RHICmdList.Transition(FRHITransitionInfo(SourceTextureRHI, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(DestinationTextureRHI, ERHIAccess::Unknown, ERHIAccess::RTV));

	FRHIRenderPassInfo RPInfo(DestinationTextureRHI, ERenderTargetActions::Load_Store);

	RHICmdList.BeginRenderPass(RPInfo, TEXT("Vertical Filtering"));

	{
		RHICmdList.SetViewport(0, 0, 0.0f, DestinationTextureRHI->GetSizeX(), DestinationTextureRHI->GetSizeY(), 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		// New engine version...
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FDrawQuadVS> VertexShader(ShaderMap);
		TShaderMapRef<FDrawQuadGaussianBlurVerticalPS> PixelShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

#if ENGINE_MAJOR_VERSION >= 5
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
#else
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
#endif


		PixelShader->SetFilterTexture(RHICmdList, SourceTextureRHI);

		RendererModule->DrawRectangle(RHICmdList,
			0, 0,				             // Dest X, Y
			DestinationTextureRHI->GetSizeX(),  // Dest Width
			DestinationTextureRHI->GetSizeY(),  // Dest Height
			0, 0,                            // Source U, V
			1, 1,                            // Source USize, VSize
			DestinationTextureRHI->GetSizeXY(), // Target buffer size
			FIntPoint(1, 1),                 // Source texture size
			VertexShader, EDRF_Default);
	}

	RHICmdList.EndRenderPass();
}


void FFlipFilterRenderTarget::DoFilter_CopyThrough(const FTexture2DRHIRef& SourceTextureRHI, const FTexture2DRHIRef& DestinationTextureRHI, FRHICommandListImmediate& RHICmdList, IRendererModule* RendererModule)
{
	// Need this for Vulkan
	RHICmdList.Transition(FRHITransitionInfo(SourceTextureRHI, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(DestinationTextureRHI, ERHIAccess::Unknown, ERHIAccess::RTV));

	FRHIRenderPassInfo RPInfo(DestinationTextureRHI, ERenderTargetActions::Load_Store);

	RHICmdList.BeginRenderPass(RPInfo, TEXT("Copy Through"));

	{
		RHICmdList.SetViewport(0, 0, 0.0f, DestinationTextureRHI->GetSizeX(), DestinationTextureRHI->GetSizeY(), 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		// New engine version...
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FDrawQuadVS> VertexShader(ShaderMap);
		TShaderMapRef<FDrawQuadPS> PixelShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

#if ENGINE_MAJOR_VERSION >= 5
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
#else
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
#endif


		PixelShader->SetSourceTexture(RHICmdList, SourceTextureRHI);

		RendererModule->DrawRectangle(RHICmdList,
			0, 0,				             // Dest X, Y
			DestinationTextureRHI->GetSizeX(),  // Dest Width
			DestinationTextureRHI->GetSizeY(),  // Dest Height
			0, 0,                            // Source U, V
			1, 1,                            // Source USize, VSize
			DestinationTextureRHI->GetSizeXY(), // Target buffer size
			FIntPoint(1, 1),                 // Source texture size
			VertexShader, EDRF_Default);
	}

	RHICmdList.EndRenderPass();
	
	RHICmdList.Transition(FRHITransitionInfo(DestinationTextureRHI, ERHIAccess::RTV, ERHIAccess::SRVMask));
}
