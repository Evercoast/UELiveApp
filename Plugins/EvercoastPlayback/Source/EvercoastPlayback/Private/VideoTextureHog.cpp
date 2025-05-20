#include "VideoTextureHog.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "UObject/Object.h"
#include "UObject/Package.h"

UTextureRecord::UTextureRecord(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	texture = nullptr;
	frameIndex = -1;
	frameTimestamp = -1;
	isUsed = false;
}

void UTextureRecord::InitTexture(int width, int height, int index, EPixelFormat format)
{
	if (!texture)
	{
		texture = UTexture2D::CreateTransient(width, height, format);
		texture->UpdateResource();

		// explicitly add to root to prevent GC
		texture->AddToRoot();
	}
}

void UTextureRecord::InitRenderTargetableTexture(int width, int height, int index, EPixelFormat format)
{
	if (!texture)
	{
		auto renderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
		renderTarget->InitCustomFormat(width, height, format, false);
		renderTarget->ClearColor = FLinearColor::Gray;
		renderTarget->TargetGamma = 2.2f;
		renderTarget->UpdateResourceImmediate();
		

		texture = renderTarget;

		// explicitly add to root to prevent GC, depending on the usage of this class, the GC system might
		// destroy the texture before the client class destructor was called
		texture->AddToRoot();
	}
}

void UTextureRecord::FreeTexture()
{
	if (texture)
	{
		texture->ReleaseResource();
        
        texture->RemoveFromRoot();
		
		texture = nullptr;
	}

	frameIndex = -1;
	frameTimestamp = -1;
	isUsed = false;
}


bool UVideoTextureHog::OpenFile(const FString& filePath)
{
	return false;
}

bool UVideoTextureHog::OpenUrl(const FString& url)
{
	return false;
}

bool UVideoTextureHog::OpenSource(UMediaSource* source)
{
	return false;
}

bool UVideoTextureHog::IsVideoOpened()
{
	return false;
}
bool UVideoTextureHog::Close()
{
	return false;
}
bool UVideoTextureHog::ResetTo(double timestmap, const std::function<void()>& callback)
{
	return false;
}
bool UVideoTextureHog::StartHogging()
{
	return false;
}
bool UVideoTextureHog::StopHogging()
{
	return false;
}
bool UVideoTextureHog::IsHogging() const
{
	return false;
}
bool UVideoTextureHog::IsEndReached() const
{
	return false;
}
bool UVideoTextureHog::IsFull() const
{
	return false;
}
bool UVideoTextureHog::IsFrameIndexWithinDuration(int64_t frameIndex) const
{
	return false;
}
bool UVideoTextureHog::IsHoggingPausedDueToFull() const
{
	return false;
}
void UVideoTextureHog::Tick(UWorld* world)
{

}
float UVideoTextureHog::GetVideoDuration() const
{
	return 0;
}

void UVideoTextureHog::Destroy()
{
    
}

UTexture* UVideoTextureHog::QueryTextureAtIndex(int64_t frameIndex) const
{
	return nullptr;
}
// mark textures from the start till the requested texture as invalid
bool UVideoTextureHog::InvalidateTextureAndBefore(UTexture* pTex)
{
	return false;
}
// mark textures from the start till the texture which has the requested frame index as invalid
bool UVideoTextureHog::InvalidateTextureAndBeforeByFrameIndex(int64_t frameIndex)
{
	return false;
}
// mark all textures as invalid
void UVideoTextureHog::InvalidateAllTextures()
{

}
// -----|<--- f --->|-----
bool UVideoTextureHog::IsFrameWithinCachedRange(int64_t frameIndex) const
{
	return false;
}
// -----|<--------->|--f--
bool UVideoTextureHog::IsFrameBeyondCachedRange(int64_t frameIndex) const
{
	return false;
}

// --f--|<--------->|-----
bool UVideoTextureHog::IsFrameBeforeCachedRange(int64_t frameIndex) const
{
	return false;
}

void UVideoTextureHog::TrimCache(double medianTimestamp, double halfFrameInterval)
{

}

bool UVideoTextureHog::IsTextureBeyondRange(double timestamp, double halfFrameInterval) const
{
	return false;
}