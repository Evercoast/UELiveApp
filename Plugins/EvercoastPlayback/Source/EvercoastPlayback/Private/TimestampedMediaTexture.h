#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "MediaTexture.h"
#include "IMediaTimeSource.h"
#include "TimestampedMediaTexture.generated.h"


class IMediaTextureSample;
class FMyMediaTextureClockSink;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMediaTextureSampleUpdated, double, timestamp);


UCLASS()
class UTimestampedMediaTexture : public UMediaTexture
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY()
	FOnMediaTextureSampleUpdated OnSampleUpdated;

public:
	FMediaTimeStamp GetTime() const;
	int64_t GetFrameIndex(double frameRate) const;
public:
	//~ UTexture interface.
	virtual FTextureResource* CreateResource() override;

	// Overriding so no UFUNCTION
	void SetMediaPlayer(UMediaPlayer* NewMediaPlayer);

protected:
	void UpdateSampleReceiver();
	void SaveSampleInfo(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample);

private:
	void TickMyResource(FTimespan Timecode);

private:
	// steal clock sink
	friend class FMyMediaTextureClockSink;
	TSharedPtr<FMyMediaTextureClockSink, ESPMode::ThreadSafe> MyClockSink;
	FGuid CurrentGuid;
	FGuid DefaultGuid;
	/** The player that is currently associated with this texture. */
	TWeakObjectPtr<UMediaPlayer> CurrentPlayer;

	TSharedPtr<FMediaTextureSampleQueue, ESPMode::ThreadSafe> m_sampleReceiver;

	TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> m_mediaTextureSample;
};