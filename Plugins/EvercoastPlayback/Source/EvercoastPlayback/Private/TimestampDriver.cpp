#include "TimestampDriver.h"
#include "GhostTreeFormatReader.h"
#include "RuntimeAudio.h"

FAudioTimer::~FAudioTimer()
{
	if (m_audioComp)
	{
		m_audioComp->RemoveFromRoot();
		m_audioComp = nullptr;
	}
}

void FAudioTimer::SetAudioComponent(UAudioComponent* audioComponent)
{
	if (m_audioComp)
	{
		m_audioComp->RemoveFromRoot();
	}

	m_audioComp = audioComponent;
	m_audioComp->AddToRoot();

	if (m_callbackHandle.IsValid())
	{
		m_audioComp->OnAudioPlaybackPercentNative.Remove(m_callbackHandle);
		m_callbackHandle.Reset();
	}

	m_callbackHandle = m_audioComp->OnAudioPlaybackPercentNative.AddSP(this, &FAudioTimer::OnAudioPlaybackPercentage);
	/*
	m_callbackHandle = m_audioComp->OnAudioPlaybackPercentNative.AddLambda(
		[self=this](const UAudioComponent* InComponent, const USoundWave* InSoundWave, const float InPlaybackPercentage) {

			if (InSoundWave)
				self->m_audioTimestamp = InSoundWave->GetDuration() * InPlaybackPercentage;
		}
	);
	*/
}

void FAudioTimer::OnAudioPlaybackPercentage(const UAudioComponent* InComponent, const USoundWave* InSoundWave, const float InPlaybackPercentage)
{
	if (InSoundWave)
		m_audioTimestamp = InSoundWave->GetDuration() * InPlaybackPercentage;
}

float FAudioTimer::GetElapsedTime() const
{
	// Check if the sound was runtime imported, or offline assigned
	if (m_audioComp && m_audioComp->Sound)
	{
		if (m_audioComp->Sound->IsA<URuntimeAudio>())
		{
			URuntimeAudio* soundWave = static_cast<URuntimeAudio*>(m_audioComp->Sound);
			return soundWave->GetPlaybackTime();
		}
	}

	return m_audioTimestamp;
}

void FEvercoastSequencerOverrideTimer::SetOverrideTime(float overrideCurrTime, float blockOnTime)
{
	m_overrideCurrTime = overrideCurrTime;
	m_blockOnTime = blockOnTime;
}


float FEvercoastSequencerOverrideTimer::GetElapsedTime() const
{
	return std::min(m_overrideCurrTime - m_loopedTime, m_blockOnTime - m_loopedTime);
}

void FEvercoastSequencerOverrideTimer::RecordLoop()
{
	m_loopedTime += m_duration;
}

void FEvercoastSequencerOverrideTimer::ResetTimer()
{
	m_overrideCurrTime = -1;
	m_blockOnTime = std::numeric_limits<float>::max();
	RecalcLoopCount();
}

void FEvercoastSequencerOverrideTimer::RecalcLoopCount()
{
	if (m_overrideCurrTime >= 0)
	{
		int loopCycles = (int)(m_overrideCurrTime / m_duration);
		m_loopedTime = loopCycles * m_duration;
	}
	else
		m_loopedTime = 0;
}

FTimestampDriver::FTimestampDriver(float duration) :
	m_sequencerTimer(duration),
	m_baseMode(BASEMODE_INVALID),
	m_overrideMode(OVERRIDEMODE_NONE),
	m_clipDuration(duration)
{
	m_audioTimer = MakeShared<FAudioTimer>();
}

void FTimestampDriver::EnterSequencerTimestampOverride(float overrideTimestamp, float blockOnTime)
{
	m_overrideMode = OVERRIDEMODE_SEQUENCER;
	m_sequencerTimer.SetOverrideTime(overrideTimestamp, blockOnTime);
}

void FTimestampDriver::ExitSequencerTimestampOverride()
{
	m_overrideMode = OVERRIDEMODE_NONE;
	m_sequencerTimer.ResetTimer();
	m_worldTimer.ResetTimer();
	m_audioTimer->ResetTimer();
}

bool FTimestampDriver::IsSequencerOverriding() const
{
	return m_overrideMode == OVERRIDEMODE_SEQUENCER;
}

void FTimestampDriver::RecalcSequencerTimestampOverrideLoopCount()
{
	m_sequencerTimer.RecalcLoopCount();
}

void FTimestampDriver::ForceChangeVideoDuration(float newDuration)
{
	switch (m_overrideMode)
	{
	case OVERRIDEMODE_NONE:
		break;
	case OVERRIDEMODE_SEQUENCER:
		m_sequencerTimer.m_duration = newDuration;
		m_sequencerTimer.RecalcLoopCount(); // duration is affected, recalc the loop number

		break;
	}

	m_clipDuration = newDuration;
}

void FTimestampDriver::UseAudioTimestamps(UAudioComponent* audioComponent)
{
	m_baseMode = BASEMODE_AUDIO;

	m_audioTimer->SetAudioComponent(audioComponent);
	ResetTimer();
}

void FTimestampDriver::UseWorldTimestamps(UWorld* world)
{
	m_baseMode = BASEMODE_WORLD;

	m_worldTimer.SetWorld(world);
	ResetTimer();
}

void FTimestampDriver::ResetTimer()
{
	switch (m_baseMode)
	{
	case BASEMODE_WORLD:
		m_worldTimer.ResetTimer();
		break;
	case BASEMODE_AUDIO:
		break;
	default:
		UE_LOG(EvercoastReaderLog, Error, TEXT("Uninitialised TimestampDriver"));
	}

	switch (m_overrideMode)
	{
	case OVERRIDEMODE_NONE:
		break;
	case OVERRIDEMODE_SEQUENCER:
		// there's no defined behaviour for sequencer timer to reset, it will always be set by the sequencer
		break;
	}
}

void FTimestampDriver::ResetTimerTo(float time, bool startAfterReset)
{
	switch (m_baseMode)
	{
	case BASEMODE_WORLD:
		m_worldTimer.ResetTimerTo(time, startAfterReset);
		break;
	case BASEMODE_AUDIO:
		break;
	default:
		UE_LOG(EvercoastReaderLog, Error, TEXT("Uninitialised TimestampDriver"));
	}

	switch (m_overrideMode)
	{
	case OVERRIDEMODE_NONE:
		break;
	case OVERRIDEMODE_SEQUENCER:
		// there's no defined behaviour for sequencer timer to reset to a new timestamp
		break;
	}
}

void FTimestampDriver::Start()
{
	switch (m_baseMode)
	{
	case BASEMODE_WORLD:
		m_worldTimer.Start();
		break;
	case BASEMODE_AUDIO:
		break;
	default:
		UE_LOG(EvercoastReaderLog, Error, TEXT("Uninitialised TimestampDriver"));
	}
}

void FTimestampDriver::Pause()
{
	switch (m_baseMode)
	{
	case BASEMODE_WORLD:
		m_worldTimer.Pause();
		break;
	case BASEMODE_AUDIO:
		break;
	default:
		UE_LOG(EvercoastReaderLog, Error, TEXT("Uninitialised TimestampDriver"));
	}
}

void FTimestampDriver::MarkLoop()
{
	switch (m_baseMode)
	{
	case BASEMODE_WORLD:
		break;
	case BASEMODE_AUDIO:
		break;
	default:
		UE_LOG(EvercoastReaderLog, Error, TEXT("Uninitialised TimestampDriver"));
	}

	switch (m_overrideMode)
	{
	case OVERRIDEMODE_NONE:
		break;
	case OVERRIDEMODE_SEQUENCER:
		m_sequencerTimer.RecordLoop();
		break;
	}

}

float FTimestampDriver::GetElapsedTime() const
{
	switch (m_overrideMode)
	{
	case OVERRIDEMODE_NONE:
		break;
	case OVERRIDEMODE_SEQUENCER:
		return m_sequencerTimer.GetElapsedTime();
	}

	switch (m_baseMode)
	{
	case BASEMODE_WORLD:
		return m_worldTimer.GetElapsedTime();
		break;
	case BASEMODE_AUDIO:
		return m_audioTimer->GetElapsedTime();
		break;
	default:
		UE_LOG(EvercoastReaderLog, Error, TEXT("Uninitialised TimestampDriver"));
	}

	return 0;
}

float FTimestampDriver::GetVideoDuration() const
{
	switch (m_overrideMode)
	{
	case OVERRIDEMODE_NONE:
		break;
	case OVERRIDEMODE_SEQUENCER:
		return m_sequencerTimer.m_duration;
	}

	return m_clipDuration;
}