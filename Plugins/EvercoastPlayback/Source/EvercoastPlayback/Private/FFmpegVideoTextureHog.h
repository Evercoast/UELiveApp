#pragma once
#include "CoreMinimal.h"
#include <inttypes.h>
#include <future>
#include <functional> 
#include <mutex>
#include <condition_variable>

#include "VideoTextureHog.h"
#include "FFmpegVideoTextureHog.generated.h"

#define FORCE_NV12_CPU_CONVERSION (0)

class UTextureRecord;
class FFFmpegDecodingThread;

UCLASS()
class UFFmpegVideoTextureHog : public UVideoTextureHog
{
	GENERATED_BODY()

public:
    UFFmpegVideoTextureHog(const FObjectInitializer& ObjectInitializer);
    virtual ~UFFmpegVideoTextureHog();

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
	void OnVideoOpened(int64_t avformat_duration, int32_t frame_rate, int frame_width, int frame_height);
	void OnVideoEndReached(int64_t last_frame_index);
	void OnConvertNV12Texture(double timestamp, int64_t frame_index, int64_t frame_pts, int width, int height, uint32_t y_pitch, uint32_t u_pitch, uint32_t v_pitch, uint8_t* y_data, uint8_t* u_data, uint8_t* v_data);
	void OnFull(bool isFull);

	void RestartHoggingIfPausedDueToFull();
	void DrainRHICommandList();

	struct VideoOpenParams
	{
		bool			isPending = false;
		int64_t			avformatDuration = 0;
		int32_t			frameRate = 0;
		int				frameWidth = 0;
		int				frameHeight = 0;
	};
	VideoOpenParams		m_videoOpenParams;
	bool				m_videoOpened;
	bool				m_videoEndReached;
	int64_t				m_videoEndFrameIndex;
	bool				m_hoggingStoppedDueToFullBuffer;

    UPROPERTY(Transient)
	TArray<UTextureRecord*> m_textureBuffer;
    
	int 				RING_QUEUE_SIZE = 30; // this could change based on the content opened
	int32_t				m_videoFrameRate = -1;
	int32_t 			m_textureBufferStart;
	int32_t 			m_textureBufferEnd;
	
	double				m_videoDuration = 0;

#if FORCE_NV12_CPU_CONVERSION
	uint8_t*			m_scratchPadRGBA;
#else
	FTexture2DRHIRef	m_nv12YPlaneRHI;
	FTexture2DRHIRef	m_nv12UPlaneRHI;
	FTexture2DRHIRef	m_nv12VPlaneRHI;
#endif
	// conversion
	uint8* m_scratchPadY;
	uint8* m_scratchPadU;
	uint8* m_scratchPadV;

	// threading
	FFFmpegDecodingThread*		m_runnable;
	FRunnableThread*			m_runnableController;
	mutable std::mutex			m_textureRecordMutex;
	mutable std::mutex			m_controlBitMutex;

	std::promise<void>			m_renderThreadPromise;
	std::future<void>			m_renderThreadFuture;
};
