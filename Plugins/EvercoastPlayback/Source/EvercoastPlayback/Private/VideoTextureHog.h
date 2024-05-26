#pragma once

#include "EvercoastHeader.h"
#include <inttypes.h>
#include <future>
#include <functional> 
#include "VideoTextureHog.generated.h"

class UMediaSource;
class UTexture;

UCLASS()
class UTextureRecord : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(Transient)
	UTexture* texture = nullptr;

	int64_t frameIndex = -1;
	double frameTimestamp = -1.0;
	bool isUsed = false;

	void InitTexture(int width, int height, int index, EPixelFormat format = EPixelFormat::PF_B8G8R8A8);
	void InitRenderTargetableTexture(int width, int height, int index, EPixelFormat format = EPixelFormat::PF_B8G8R8A8);
	void FreeTexture();
	void MarkAsUsed(bool used)
	{
		isUsed = used;
	}

	void SetFrameTimestamp(int64_t fIndex, double fTimestamp)
	{
		this->frameIndex = fIndex;
		this->frameTimestamp = fTimestamp;
	}
	
};


UCLASS(Abstract)
class UVideoTextureHog : public UObject
{
	GENERATED_BODY()

public:
	virtual bool OpenFile(const FString& filePath);
	virtual bool OpenUrl(const FString& url);
	virtual bool OpenSource(UMediaSource* source);
	virtual bool Close();
	virtual bool IsVideoOpened();
	virtual bool ResetTo(double timestmap, const std::function<void()>& callback = [](){} );
	virtual bool JumpBy(double timestampOffset, const std::function<void()>& callback = []() {});
	virtual bool StartHogging();
	virtual bool StopHogging();
	virtual bool IsHogging() const;
	virtual bool IsEndReached() const;
	virtual bool IsFull() const;
	virtual bool IsFrameIndexWithinDuration(int64_t frameIndex) const;
	virtual bool IsHoggingPausedDueToFull() const;
	virtual void Tick(UWorld* world);
	virtual float GetVideoDuration() const;
    virtual void Destroy();

	virtual UTexture* QueryTextureAtIndex(int64_t frameIndex) const;
	// mark textures from the start till the requested texture as invalid
	virtual bool InvalidateTextureAndBefore(UTexture* pTex);
	// mark textures from the start till the texture which has the requested frame index as invalid
	virtual bool InvalidateTextureAndBeforeByFrameIndex(int64_t frameIndex);
	// mark all textures as invalid
	virtual void InvalidateAllTextures();
	// -----|<--- f --->|-----
	virtual bool IsFrameWithinCachedRange(int64_t frameIndex) const;
	// -----|<--------->|--f--
	virtual bool IsFrameBeyondCachedRange(int64_t frameIndex) const;
	// --f--|<--------->|-----
	virtual bool IsFrameBeforeCachedRange(int64_t frameIndex) const;
};
