#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "UObject/SoftObjectPtr.h"
#include "Sound/SoundWave.h"
#include "Components/AudioComponent.h"
#include <numeric>
#include <vector>

struct FEvercoastPlaybackTimer
{
	UWorld* m_world;
	float m_startTime;
	float m_lastPlaybackSessionElapsedTime;
	bool m_started;
	FEvercoastPlaybackTimer() :
		m_world(nullptr),
		m_startTime(0),
		m_lastPlaybackSessionElapsedTime(0),
		m_started(false)
	{
	}

	void SetWorld(UWorld* world)
	{
		m_world = world;
	}


	void Start()
	{
		if (!m_world)
			return;

		if (!m_started)
		{
			m_startTime = m_world->GetTimeSeconds();
			m_started = true;
		}
	}
	void Pause()
	{
		if (!m_world)
			return;

		if (m_started)
		{
			m_lastPlaybackSessionElapsedTime += m_world->GetTimeSeconds() - m_startTime;
			m_started = false;
		}
	}

	void ResetTimer()
	{
		m_startTime = 0;
		m_lastPlaybackSessionElapsedTime = 0;
		m_started = false;
	}

	void ResetTimerTo(float forceTimestamp, bool startTimerAfterReset)
	{
		m_startTime = 0;
		m_lastPlaybackSessionElapsedTime = forceTimestamp;
		m_started = false;

		if (startTimerAfterReset)
			Start();
	}

	float GetElapsedTime() const
	{
		if (m_started)
			return m_lastPlaybackSessionElapsedTime + (m_world->GetTimeSeconds() - m_startTime);

		return m_lastPlaybackSessionElapsedTime;
	}
};

struct FEvercoastSequencerOverrideTimer
{
	FEvercoastSequencerOverrideTimer(float duration) :
		m_duration(duration),
		m_overrideCurrTime(-1),
		m_blockOnTime(std::numeric_limits<float>::max()),
		m_loopedTime(0)
	{

	}

	bool IsValid() const
	{
		return m_overrideCurrTime >= 0;
	}

	void SetOverrideTime(float overrideCurrTime, float blockOnTime);

	float GetElapsedTime() const;

	void RecordLoop();

	void ResetTimer();

	void RecalcLoopCount();

	float m_duration;
	float m_overrideCurrTime;
	float m_blockOnTime;
	float m_loopedTime;
};

struct FAudioTimer : public TSharedFromThis<FAudioTimer>
{
	FAudioTimer() :
		m_audioComp(nullptr),
		m_audioTimestamp(0)
		
	{
	}

	~FAudioTimer();

	void SetAudioComponent(UAudioComponent* audioComponent);
	float GetElapsedTime() const;
	void ResetTimer()
	{
		m_audioTimestamp = 0;
	}


	UAudioComponent*					m_audioComp;
	float								m_audioTimestamp;
	FDelegateHandle						m_callbackHandle;
private:
	void OnAudioPlaybackPercentage(const UAudioComponent* InComponent, const USoundWave* InSoundWave, const float InPlaybackPercentage);

};

class FTimestampDriver
{
public:
	FTimestampDriver(float clipDuration);
	
	void UseAudioTimestamps(UAudioComponent* audioComponent);
	void UseWorldTimestamps(UWorld* world);
	void EnterSequencerTimestampOverride(float overrideTimestamp, float blockOnTime);
	void ExitSequencerTimestampOverride();
	bool IsSequencerOverriding() const;
	void RecalcSequencerTimestampOverrideLoopCount();
	void ForceChangeVideoDuration(float newDuration); // this is added for compensate the possible inconsistency between geometry/video duration while keeping the system async

	void Start();
	void Pause();
	void MarkLoop(); // <- will mark passed the loop for some timers
	void ResetTimer();
	void ResetTimerTo(float timestamp, bool startAfterReset);
	float GetElapsedTime() const;
	float GetVideoDuration() const;

private:

	enum BaseMode
	{
		BASEMODE_INVALID = 0,
		BASEMODE_WORLD,
		BASEMODE_AUDIO,
		BASEMODE_NUM
	};

	enum OverrideMode
	{
		OVERRIDEMODE_NONE = 0,
		OVERRIDEMODE_SEQUENCER,
		OVERRIDEMODE_NUM
	};
	
	FEvercoastPlaybackTimer				m_worldTimer;
	TSharedPtr<FAudioTimer>				m_audioTimer;
	FEvercoastSequencerOverrideTimer	m_sequencerTimer;
	
	BaseMode							m_baseMode;
	OverrideMode						m_overrideMode;


	float								m_clipDuration;
};