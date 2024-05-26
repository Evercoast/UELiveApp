#pragma once

#include "CoreMinimal.h"
#include <inttypes.h>
#include <future>
#include <functional> 

#include "VideoTextureHog.h"
#include "ElectraVideoTextureHog.generated.h"


class UTimestampedMediaTexture;
class UMediaPlayer;
class UMediaSource;

UCLASS()
class UElectraVideoTextureHog : public UVideoTextureHog
{
	GENERATED_BODY()

public:
    UElectraVideoTextureHog(const FObjectInitializer& ObjectInitializer);
    virtual ~UElectraVideoTextureHog();

    virtual bool OpenFile(const FString& filePath) override;
	virtual bool OpenUrl(const FString& url) override;
	virtual bool OpenSource(UMediaSource* source) override;
	virtual bool IsVideoOpened() override;
	virtual bool Close() override;
	virtual bool ResetTo(double timestmap, const std::function<void()>& callback = [](){} ) override;
	virtual bool JumpBy(double timestampOffset, const std::function<void()>& callback = []() {}) override;
	virtual bool StartHogging() override;
	virtual bool StopHogging() override;
	virtual bool IsHogging() const override;
	virtual bool IsEndReached() const override;
	virtual bool IsFull() const override;
	virtual bool IsFrameIndexWithinDuration(int64_t frameIndex) const override;
	virtual bool IsHoggingPausedDueToFull() const override;
	virtual void Tick(UWorld* world) override;
	virtual float GetVideoDuration() const override;
    virtual void Destroy() override;

	UMediaPlayer* GetMediaPlayer() const
	{
		return m_mediaPlayer;
	}

	virtual UTexture* QueryTextureAtIndex(int64_t frameIndex) const override;
	// mark textures from the start till the requested texture as invalid
	virtual bool InvalidateTextureAndBefore(UTexture* pTex) override;
	// mark textures from the start till the texture which has the requested frame index as invalid
	virtual bool InvalidateTextureAndBeforeByFrameIndex(int64_t frameIndex) override;
	// mark all textures as invalid
	virtual void InvalidateAllTextures() override;
	// -----|<--- f --->|-----
	virtual bool IsFrameWithinCachedRange(int64_t frameIndex) const override;
	// -----|<--------->|--f--
	virtual bool IsFrameBeyondCachedRange(int64_t frameIndex) const override;
	// --f--|<--------->|-----
	virtual bool IsFrameBeforeCachedRange(int64_t frameIndex) const override;


private:
	int RING_QUEUE_SIZE = 30; // this could change based on the content opened
	void PrepareForOpening();
	bool HogCurrentFrame();
	int64_t GetCurrentFrameIndex() const;
	double GetCurrentFrameTimestamp() const;
	void HogCurrentFrameAndDoBookkeeping();
	bool PauseHoggingDueToFull();
	void RestartHoggingIfPausedDueToFull();
	bool IsTryingFastforwardToAccurateSeek() const;

	UFUNCTION()
	void OnMediaOpened(FString openedUrl);

	UFUNCTION()
	void OnMediaOpenFailed(FString failedUrl);

	UFUNCTION()
	void OnMediaTextureSampleUpdated(double timestamp);

	UFUNCTION()
	void OnMediaSeekCompleted();

	UFUNCTION()
	void OnMediaEndReached();


	UPROPERTY(Transient)
	UMediaPlayer* m_mediaPlayer;

	UPROPERTY(Transient)
	UTimestampedMediaTexture* m_mediaTexture;
	

	UPROPERTY(Transient)
	TArray<UTextureRecord*> m_textureBuffer;

	UPROPERTY(Transient)
	UMediaSource* m_lastMediaSource;

	TSharedPtr<TPromise<void>, ESPMode::ThreadSafe> m_promise;

	int32_t m_textureBufferStart;
	int32_t m_textureBufferEnd;

	float m_mediaFrameRate;
	int64_t m_lastFrameIndex;

	bool m_mediaOpened;
	std::promise<void> m_mediaOpenPromise;
	std::future<void> m_mediaOpenFuture;
	bool m_mediaEndReached;
	enum MediaSeekStatus
	{
		Seek_Uninitialised,
		Seek_Requested,
		Seek_Completed_BigGap,
		Seek_TexUpdated_BigGap,
		Seek_Completed
	};
	MediaSeekStatus m_mediaSeekStatus;
	double m_mediaPendingSeek;
	double m_lastMediaTextureTimestamp;

	bool m_hoggingStoppedDueToFullBuffer;
	std::function<void()> m_seekingCompletedCallback;

#if PLATFORM_ANDROID
	enum MediaPlayerStatus
	{
		MPS_STOPPED,
		MPS_PLAYING,
	};

	MediaPlayerStatus m_supposedMediaPlayerStatus;
	float m_lastMediaPlayerStatusCheckTime;

	void CheckMediaPlayerStatus(UWorld* world);
#endif
};
