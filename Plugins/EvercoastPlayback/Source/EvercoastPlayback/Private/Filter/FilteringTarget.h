#pragma once
#include "CoreMinimal.h"
#include "RenderResource.h"
#include "RHIResources.h"
#include "UnrealEngineCompatibility.h"

class FGenericDepthTarget : public FRenderResource
{
public:
	void Init(int32 width, int32 height)
	{
		this->Width = width;
		this->Height = height;
	}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
#else
	virtual void InitRHI() override
#endif
	{
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 1
		FRHIResourceCreateInfo CreateInfo(TEXT("FGenericDepthTarget"), FClearValueBinding(0, 1));
		FRHITextureCreateDesc desc = FRHITextureCreateDesc::Create2D(CreateInfo.DebugName, Width, Height, EPixelFormat::PF_DepthStencil)
			.SetNumMips(1).SetNumSamples(1)
			.SetFlags(ETextureCreateFlags::DepthStencilTargetable | ETextureCreateFlags::Dynamic)
			.SetClearValue(CreateInfo.ClearValueBinding);
		DepthTextureRHI = RHICreateTexture(desc);
#else
		FRHIResourceCreateInfo CreateInfo(TEXT("FGenericDepthTarget"), FClearValueBinding(0, 1));
		DepthTextureRHI = RHICreateTexture2D(Width, Height,
			EPixelFormat::PF_DepthStencil, 1, 1,
			ETextureCreateFlags::DepthStencilTargetable | ETextureCreateFlags::Dynamic,
			CreateInfo);
#endif
#else
		FRHIResourceCreateInfo CreateInfo(FClearValueBinding(0, 1));
		DepthTextureRHI = RHICreateTexture2D(Width, Height,
			EPixelFormat::PF_DepthStencil, 1, 1,
			ETextureCreateFlags::TexCreate_DepthStencilTargetable | ETextureCreateFlags::TexCreate_Dynamic,
			CreateInfo);
#endif

	}

	virtual void ReleaseRHI() override
	{
		DepthTextureRHI.SafeRelease();
	}

	FTexture2DRHIRef DepthTextureRHI;

private:
	int32 Width = 1024;
	int32 Height = 1024;
};

class FFlipFilterRenderTarget : public FRenderResource
{
public:
	void Init(int32 width, int32 height, EPixelFormat format)
	{
		Width = width;
		Height = height;
		PixelFormat = format;
	}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
#else
	virtual void InitRHI() override
#endif
	{
#if ENGINE_MAJOR_VERSION == 5
		FRHIResourceCreateInfo CreateInfo(TEXT("FFlipFilterRenderTarget"), FClearValueBinding(FLinearColor(0, 1, 0, 1)));
#if ENGINE_MINOR_VERSION >= 1
		for (int i = 0; i < 2; ++i)
		{
			FRHITextureCreateDesc desc = FRHITextureCreateDesc::Create2D(CreateInfo.DebugName, Width, Height, PixelFormat)
				.SetNumMips(1).SetNumSamples(1)
				.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::Dynamic)
				.SetClearValue(CreateInfo.ClearValueBinding);

			FlipTargets[i] = RHICreateTexture(desc);
		}
#else
		for (int i = 0; i < 2; ++i)
		{
			FlipTargets[i] = RHICreateTexture2D(Width, Height,
				PixelFormat, 1, 1,
				ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::Dynamic,
				CreateInfo);
		}
#endif
#else
		FRHIResourceCreateInfo CreateInfo(FClearValueBinding(FLinearColor(0, 1, 0, 1)));
		for (int i = 0; i < 2; ++i)
		{
			FlipTargets[i] = RHICreateTexture2D(Width, Height,
				PixelFormat, 1, 1,
				ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_Dynamic,
				CreateInfo);
		}
#endif
	}

	virtual void ReleaseRHI() override
	{
		for (int i = 0; i < 2; ++i)
		{
			FlipTargets[i].SafeRelease();
			FlipTargets[i] = nullptr;
		}
	}

	FTexture2DRHIRef GetFilterTarget(int i)
	{
		return FlipTargets[i];
	}

	void DoFilter(const FTexture& SrcTexture, const FTexture& DstTexture, int FlipTimes, FRHICommandListImmediate& RHICmdList, IRendererModule* RendererModule) const
	{
		if (FlipTimes > 0)
		{
			DoFilter_Horizontal(SrcTexture.TextureRHI->GetTexture2D(), FlipTargets[0], RHICmdList, RendererModule);

			for (int i = 0; i < FlipTimes - 1; ++i)
			{
				DoFilter_Vertical(FlipTargets[0], FlipTargets[1], RHICmdList, RendererModule);
				DoFilter_Horizontal(FlipTargets[1], FlipTargets[0], RHICmdList, RendererModule);
			}

			DoFilter_Vertical(FlipTargets[0], FlipTargets[1], RHICmdList, RendererModule);

			// Copy to dest texture from FlipTargets[0]
			FTextureRHIRef targetRHI = DstTexture.TextureRHI;;
			FTextureRHIRef srcRHI = FlipTargets[1]->GetTexture2D();

			RHICmdList.Transition(FRHITransitionInfo(srcRHI, ERHIAccess::Unknown, ERHIAccess::CopySrc));
			RHICmdList.Transition(FRHITransitionInfo(targetRHI, ERHIAccess::Unknown, ERHIAccess::CopyDest));
			RHICmdList.CopyTexture(srcRHI, targetRHI, FRHICopyTextureInfo());
			RHICmdList.Transition(FRHITransitionInfo(targetRHI, ERHIAccess::CopyDest, ERHIAccess::SRVMask));
			RHICmdList.Transition(FRHITransitionInfo(srcRHI, ERHIAccess::CopySrc, ERHIAccess::SRVMask));

		}
		else
		{
			// copy from src to dst, allow src and dst with different dimensions
			DoFilter_CopyThrough(SrcTexture.TextureRHI->GetTexture2D(), DstTexture.TextureRHI->GetTexture2D(), RHICmdList, RendererModule);
		}
	}

private:
	static void DoFilter_Horizontal(const FTexture2DRHIRef& SourceTextureRHI, const FTexture2DRHIRef& DestinationTextureRHI, FRHICommandListImmediate& RHICmdList, IRendererModule* RendererModule);
	static void DoFilter_Vertical(const FTexture2DRHIRef& SourceTextureRHI, const FTexture2DRHIRef& DestinationTextureRHI, FRHICommandListImmediate& RHICmdList, IRendererModule* RendererModule);
	static void DoFilter_CopyThrough(const FTexture2DRHIRef& SourceTextureRHI, const FTexture2DRHIRef& DestinationTextureRHI, FRHICommandListImmediate& RHICmdList, IRendererModule* RendererModule);

	int32 Width = 1024;
	int32 Height = 1024;
	EPixelFormat PixelFormat = EPixelFormat::PF_FloatRGBA;

	FTexture2DRHIRef FlipTargets[2] = { nullptr, nullptr };
};

