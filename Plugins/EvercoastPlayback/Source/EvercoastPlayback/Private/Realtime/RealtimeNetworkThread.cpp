#include "Realtime/RealtimeNetworkThread.h"

#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

#include "EvercoastStreamingDataDecoder.h"
#include "Realtime/RealtimeMeshingPacketHeader.h"

#include "picoquic.h"

#include "Realtime/PicoAudioSoundWave.h"
#include "EvercoastPerfCounter.h"
#include "Realtime/EvercoastRealtimeConfig.h"

#include <thread>
#include <chrono>

DEFINE_LOG_CATEGORY(EvercoastRealtimeNetworkLog);

static const char* USERNAME = "playback";
static const char* SERVER_NI = "realtime.evercoast.com";

class RealtimeRunnalble final : public FRunnable
{
public:
	struct PicoQuicFrame
	{
		uint64_t FrameNumber{0};
		uint64_t Timestamp{0};
		uint64_t TypeAndFlags{0};
		uint8_t* Data{nullptr};
		uint64_t DataSize{0};
		uint8_t* UserData{nullptr};
		uint64_t UserDataSize{0};
	};

	RealtimeRunnalble(const std::string& address, int port, const std::string& accessToken, const std::string& certificatePath, UPicoAudioSoundWave* sound,
		std::function<std::shared_ptr<IEvercoastStreamingDataDecoder>(uint32_t)> type_decision_callback,
		std::function<void(void)> failure_callback,
		std::shared_ptr<EvercoastPerfCounter> transmissionPerfCounter)
		: m_address(address)
		, m_port(port)
		, m_accessToken(accessToken)
		, m_certPath(certificatePath)
		, m_sound(sound)
		, cached_callback(type_decision_callback)
		, failure_callback(failure_callback)
		, m_transmissionPerfCounter(transmissionPerfCounter)
	{}

	~RealtimeRunnalble() final = default;

	bool Init() final
	{
		m_running = true;
		return true;
	}

	uint32 Run() final
	{
		using namespace std::chrono_literals;

		// Check if plugin module has been initialised
		while (PicoQuic::create_connection == nullptr || PicoQuic::create_connection_2 == nullptr)
		{
			std::this_thread::sleep_for(1000ms);
		}


		UEvercoastRealtimeConfig* config = NewObject<UEvercoastRealtimeConfig>();

		
#if PLATFORM_WINDOWS
		bool useOldPicoQuic = config->UseOldPicoQuic;
#else
		bool useOldPicoQuic = true;
#endif

		PicoQuic::vci_connection_handle_t geoConn, audioConn;
		if (useOldPicoQuic)
		{
			geoConn = PicoQuic::create_connection(m_address.c_str(), m_port, PicoQuic::vci_connection_type_t::vci_connection_type_receiver);
			audioConn = PicoQuic::create_connection(m_address.c_str(), m_port + 1, PicoQuic::vci_connection_type_t::vci_connection_type_receiver);
		}
		else
		{
			geoConn = PicoQuic::create_connection_2(m_address.c_str(), m_port, PicoQuic::vci_connection_type_t::vci_connection_type_receiver,
				(char*)USERNAME, (char*)m_accessToken.c_str(), m_certPath.c_str(), SERVER_NI);
			audioConn = PicoQuic::create_connection_2(m_address.c_str(), m_port + 1, PicoQuic::vci_connection_type_t::vci_connection_type_receiver,
				(char*)USERNAME, (char*)m_accessToken.c_str(), m_certPath.c_str(), SERVER_NI);
		}

		const auto connections = { geoConn, audioConn };
		uint64 startTimestamp = 0;
		while (m_running)
		{
			PicoQuic::Status currentStatus = PicoQuic::Status::Connected;
			for (const auto& conn : connections)
			{
				const auto connectionStatus = static_cast<PicoQuic::Status>(PicoQuic::get_status(conn));
				if (connectionStatus != PicoQuic::Status::Connected)
				{
					currentStatus = connectionStatus;
				}
			}
			if (currentStatus != m_status)
			{
				m_status = currentStatus;
			}

			if (useOldPicoQuic)
			{
				if (currentStatus != PicoQuic::Status::Connected)
				{
					// Wait for connection to be made
					std::this_thread::sleep_for(200ms);
					continue;
				}
			}
			else
			{
				if (currentStatus == PicoQuic::Status::FailedToAuthenticate)
				{
					// Authentication fails, bail out
					if (failure_callback)
					{
						failure_callback();
					}

					m_running = false;
					UE_LOG(EvercoastRealtimeNetworkLog, Error, TEXT("Authentication failed. Make sure the access token and server certificate are paired and correct."));
					break;
				}
				else if (currentStatus == PicoQuic::Status::Disconnected)
				{
#if PLATFORM_WINDOWS
					for (const auto& conn : connections)
					{
						const auto connectionStatus = static_cast<PicoQuic::Status>(PicoQuic::get_status(conn));
						if (connectionStatus == PicoQuic::Status::Disconnected)
						{
							UE_LOG(EvercoastRealtimeNetworkLog, Warning, TEXT("Connection %d lost. Try reconnecting..."), conn);
							// Try reconnect
							int reconnResult = PicoQuic::reconnect(conn);
							if (reconnResult == 0)
							{
								UE_LOG(EvercoastRealtimeNetworkLog, Warning, TEXT("Reconnect error: %d"), reconnResult);
							}

							std::this_thread::sleep_for(200ms);
						}
					}
#else
					std::this_thread::sleep_for(200ms);
#endif
					continue;
				}
				else if (currentStatus != PicoQuic::Status::Connected)
				{
					// Wait for connection to be made
					std::this_thread::sleep_for(200ms);
					continue;
				}
			}

			for (const auto& conn : connections)
			{
				if (PicoQuic::received_frame(conn))
				{
					if (conn == geoConn)
						m_transmissionPerfCounter->AddSample();
					
					PicoQuicFrame frame{};

					frame.FrameNumber = PicoQuic::get_frame_number(conn);
					frame.Timestamp = PicoQuic::get_timestamp(conn);
					frame.TypeAndFlags = PicoQuic::get_type_and_flags(conn);
					frame.Data = PicoQuic::get_data(conn);
					frame.DataSize = PicoQuic::get_data_size(conn);
					frame.UserData = PicoQuic::get_user_data(conn);
					frame.UserDataSize = PicoQuic::get_user_data_size(conn);

					if (startTimestamp == 0)
					{
						startTimestamp = frame.Timestamp;
					}
					double relTimetamp = static_cast<double>(static_cast<int64>(frame.Timestamp) - static_cast<int64>(startTimestamp)) * 0.001;

					// volumetric/mesh frame
					if (frame.UserDataSize == 0)
					{
						if (frame.DataSize < 1024)
						{
							// empty frame??
							UE_LOG(EvercoastRealtimeNetworkLog, Warning, TEXT("Received empty frame: %d"), frame.FrameNumber);
						}
						else
							if (frame.TypeAndFlags == 0) // main frame
							{
								RealtimePacketHeader* header = (RealtimePacketHeader*)(frame.Data);
								if (!m_decoder && cached_callback)
								{
									m_decoder = cached_callback(header->u32streamType);
									cached_callback = nullptr;
								}

								check(m_decoder);
								if (header->IsValid())
								{
									// realtime meshing or voxel
									m_decoder->Receive(relTimetamp, frame.FrameNumber, frame.Data, (size_t)frame.DataSize, 0);
								}
								else
								{
									UE_LOG(EvercoastRealtimeNetworkLog, Warning, TEXT("Unknown frame header or version: %d - %d"), header->headerType, header->headerVersion);
								}
							}
							else
							{
								UE_LOG(EvercoastRealtimeNetworkLog, Warning, TEXT("Unknown frame type: %d"), frame.TypeAndFlags);
							}
					}
					else
					{
						// audio frame
						if (frame.TypeAndFlags == 1)
						{
							if (m_sound)
							{
								m_sound->QueueAudio(relTimetamp, frame.FrameNumber, frame.UserData, frame.UserDataSize, frame.Data, frame.DataSize);
							}
							else
							{
								UE_LOG(EvercoastRealtimeNetworkLog, Verbose, TEXT("Realtime streaming has sound channel but this actor doesn't have AudioComponent."));
							}
						}
					}

					PicoQuic::pop_frame(conn);

				}
				else
				{
					//UE_LOG(EvercoastRealtimeNetworkLog, Log, TEXT("No realtime frame"));
					//std::this_thread::sleep_for(5ms);
				}
			}
		}

		UE_LOG(EvercoastRealtimeNetworkLog, Log, TEXT("Delete geo connection: %d"), geoConn);
		PicoQuic::delete_connection(geoConn);
		UE_LOG(EvercoastRealtimeNetworkLog, Log, TEXT("Delete audio connection: %d"), audioConn);
		PicoQuic::delete_connection(audioConn);

		return 0;
	}

	void Stop() final 
	{
		m_running = false;
	}

	void Exit() final 
	{ 
	}

	PicoQuic::Status GetStatus() const
	{
		return m_status;
	}
private:
	std::string m_address;
	int m_port{ 6655 };
	std::string m_accessToken;
	std::string m_certPath;

	std::shared_ptr<IEvercoastStreamingDataDecoder> m_decoder;
	UPicoAudioSoundWave* m_sound{nullptr};

	std::atomic<bool> m_running{ false };
	std::atomic<PicoQuic::Status> m_status{ PicoQuic::Status::NotYetConnected };

	std::function<std::shared_ptr<IEvercoastStreamingDataDecoder>(uint32_t)> cached_callback;
	std::function<void(void)> failure_callback;

	std::shared_ptr<EvercoastPerfCounter> m_transmissionPerfCounter;
};

bool RealtimeNetworkThread::Connect(const std::string& address, int port, const std::string& accessToken, const std::string& certificatePath, UPicoAudioSoundWave* sound,
	std::function<std::shared_ptr<IEvercoastStreamingDataDecoder>(uint32_t)> type_decision_callback, 
	std::function<void(void)> failure_callback,
	std::shared_ptr<EvercoastPerfCounter> transmissionPerfCounter)
{
	m_runnable = new RealtimeRunnalble(address, port, accessToken, certificatePath, sound, type_decision_callback, failure_callback, transmissionPerfCounter);
	m_cachedCallback = type_decision_callback;
	m_failureCallback = failure_callback;
	check(type_decision_callback);

	m_runnableController = FRunnableThread::Create(m_runnable, TEXT("Evercoast Realtime Network Thread"));
	return true;
}

void RealtimeNetworkThread::Disconnect()
{
	m_failureCallback = nullptr;
	m_cachedCallback = nullptr;
	m_runnableController->Kill(true);
	m_decoder.reset();
	delete m_runnableController;
	m_runnableController = nullptr;
	delete m_runnable;
	m_runnable = nullptr;
}

PicoQuic::Status RealtimeNetworkThread::GetStatus() const
{
	if (m_runnable)
	{
		return (PicoQuic::Status)((RealtimeRunnalble*)(m_runnable))->GetStatus();
	}

	return PicoQuic::Status::NotYetConnected;
}
