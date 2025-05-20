#pragma once

#include <string>
#include <memory>
#include <functional>
#include "picoquic.h"

class FRunnable;
class FRunnableThread;
class EvercoastPerfCounter;

class UPicoAudioSoundWave;
class IEvercoastRealtimeDataDecoder;

DECLARE_LOG_CATEGORY_EXTERN(EvercoastRealtimeNetworkLog, Log, All);

class RealtimeNetworkThread
{
public:
	bool Connect(const std::string& address, int port, const std::string& accessToken, const std::string& certificatePath, UPicoAudioSoundWave* sound,
		std::function<std::shared_ptr<IEvercoastRealtimeDataDecoder>(uint32_t)> type_decision_callback,
		std::function<void(void)> failure_callback,
		std::shared_ptr<EvercoastPerfCounter> perfCounter);
	void Disconnect();
	PicoQuic::Status GetStatus() const;

private:
	std::function<std::shared_ptr<IEvercoastRealtimeDataDecoder>(uint32_t)> m_cachedCallback;
	std::function<void(void)> m_failureCallback;
	std::shared_ptr<IEvercoastRealtimeDataDecoder> m_decoder;

	// threading
	FRunnable* m_runnable = nullptr;
	FRunnableThread* m_runnableController = nullptr;
};