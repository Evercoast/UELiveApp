#include "FFmpegVideoTextureHog.h"
#include "GhostTreeFormatReader.h"
#include "MediaSource.h"
#include "NV12Conversion.h"
#include <thread>
#include <chrono>
#include <string.h>

// FFmpeg headers are only C compatible
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}


extern TGlobalResource<FNV12ConversionDummyVertexBuffer> GNV12TextureConversionVertexBuffer;
extern TGlobalResource<FNV12ConversionDummyIndexBuffer> GNV12TextureConversionIndexBuffer;
extern TGlobalResource<FNV12TextureConversionVertexDeclaration> GNV12TextureConversionVertexDeclaration;

class FFFmpegDecodingThread final : public FRunnable
{
	AVFormatContext*		m_videoFormatCtx;
	AVIOContext*			m_videoIOCtx;
	const AVCodec*			m_videoCodec;
	AVCodecParameters*		m_videoCodecParam;
	AVCodecContext*			m_videoCodecCtx;
	AVFrame*				m_currVideoFrame;
	AVPacket*				m_currVideoPacket;
	AVRational				m_videoTimebase;
	int32_t					m_videoFrameRate = -1;
	int32_t					m_videoStreamIndex = -1;

	enum Command
	{
		CMD_NOP = 0,
		CMD_OPEN,
		CMD_DECODE,
		CMD_SEEK,
		CMD_CLOSE,
		CMD_CLOSE_THEN_STOP
	};

	struct CommandPayload
	{
		Command cmd;
		std::function<void(void)> payload;

		CommandPayload(Command cmd, const std::function<void(void)>& payload) :
			cmd(cmd), payload(payload)
		{
		}

		void RunPayload()
		{
			if (payload)
				payload();
		}
	};

	char					m_openFilename[2048];
	double					m_seekTargetTimestamp = 0;
	int64_t					m_lastHoggedFramePts = 0;
	int64_t					m_lastHoggedFrameIndex = -1;
	std::function<void(int64_t, int32_t, int, int)>	
							m_openingCompletedCallback;
	std::function<void(int64_t)>
							m_reachedStreamEndCallback;
	std::function<void(double, int64_t, int64_t, int, int, uint32_t, uint32_t, uint32_t, uint8_t*, uint8_t*, uint8_t*)>	
							m_convertTextureCallback;
	std::function<void()>	m_seekCompletedCallback;
	std::function<bool()>	m_checkBufferFullCallback;
	std::function<void(bool)> m_bufferFullCallback;

	Command					m_currCmd{ Command::CMD_NOP };
	std::deque<CommandPayload>	m_cmdQueue;
	mutable std::mutex		m_cmdQueueAccessMutex;

	std::atomic<bool>		m_running{ false };
	
	mutable std::mutex		m_cmdCondMutex;
	std::condition_variable	m_cmdCond;
	mutable std::mutex		m_semaphoreCountMutex;
	int						m_semaphoreCount { 0 };


	std::promise<void>		m_openingPromise;
	std::future<void>		m_openingFuture;

	TSharedPtr<FArchive, ESPMode::NotThreadSafe> m_fileStream;
	
	void*					m_ioBuffer = nullptr;

public:
	virtual bool Init() override
	{
		m_seekTargetTimestamp = 0;
		m_videoFormatCtx = nullptr;
		m_videoIOCtx = nullptr;
		m_videoCodec = nullptr;
		m_videoCodecParam = nullptr;
		m_videoCodecCtx = nullptr;
		m_currVideoFrame = nullptr;
		m_currVideoPacket = nullptr;
		m_videoTimebase = av_make_q(0, 1);
		m_videoFrameRate = -1;
		m_videoStreamIndex = -1;
		m_ioBuffer = nullptr;

		m_openFilename[0] = 0;
		m_currCmd = Command::CMD_NOP;
		m_running = true;

		return true;
	}

	void CommandOpenFile(const char* filename, std::function<void(int64_t, int32_t, int, int)> onOpenedCallback, std::function<void(int64_t)> onReachedVideoEndCallback,
		std::function<void(double, int64_t, int64_t, int, int, uint32_t, uint32_t, uint32_t, uint8_t*, uint8_t*, uint8_t*)> convertTextureCallback,
		std::function<bool(void)> checkBufferFullCallback, std::function<void(bool)> bufferFullCallback)
	{
		std::lock_guard<std::mutex> guard(m_cmdQueueAccessMutex);

		// Copy the string outside the lambda for sake of simplicity...
#if PLATFORM_WINDOWS
		strncpy_s(m_openFilename, 2048, filename, strlen(filename));
#else
		strncpy(m_openFilename, filename, 2048);
#endif

		m_cmdQueue.push_back(CommandPayload(Command::CMD_OPEN, [this, onOpenedCallback, onReachedVideoEndCallback, convertTextureCallback, checkBufferFullCallback, bufferFullCallback]() {
			m_seekTargetTimestamp = 0;
			m_openingCompletedCallback = onOpenedCallback;
			m_reachedStreamEndCallback = onReachedVideoEndCallback;
			m_convertTextureCallback = convertTextureCallback;
			m_checkBufferFullCallback = checkBufferFullCallback;
			m_bufferFullCallback = bufferFullCallback;

			m_openingPromise = std::promise<void>();
			m_openingFuture = m_openingPromise.get_future();
		}));

		Notify();
	}

	

	void CommandResumeDecoding()
	{
		std::lock_guard<std::mutex> guard(m_cmdQueueAccessMutex);
		m_cmdQueue.push_back(CommandPayload(Command::CMD_DECODE, nullptr));
		Notify();
	}

	void CommandPauseDecoding()
	{
		std::lock_guard<std::mutex> guard(m_cmdQueueAccessMutex);
		m_cmdQueue.push_back(CommandPayload(Command::CMD_NOP, nullptr));

		Notify();
	}

	void CommandSeekTo(double timestamp, std::function<void()> onSeekCompletedCallback )
	{
		std::lock_guard<std::mutex> guard(m_cmdQueueAccessMutex);

		m_cmdQueue.push_back(CommandPayload(Command::CMD_SEEK, [this, timestamp, onSeekCompletedCallback]() {
				m_seekTargetTimestamp = timestamp;
				m_seekCompletedCallback = onSeekCompletedCallback;
			}));
		
		Notify();
	}

	void CommandSeekRelative(double timestampOffset, std::function<void()> onSeekCompletedCallback)
	{
		std::lock_guard<std::mutex> guard(m_cmdQueueAccessMutex);

		m_cmdQueue.push_back(CommandPayload(Command::CMD_SEEK, [this, timestampOffset, onSeekCompletedCallback]() {
				double timestamp = Framenumber2Timestamp(m_lastHoggedFramePts) + timestampOffset;
				if (timestamp < 0)
					timestamp = 0;

				m_seekTargetTimestamp = timestamp;
				m_seekCompletedCallback = onSeekCompletedCallback;
			}));

		
		Notify();
	}


	void CommandClose()
	{
		SyncOpenFile();

		std::lock_guard<std::mutex> guard(m_cmdQueueAccessMutex);
		m_cmdQueue.push_back(CommandPayload(Command::CMD_CLOSE, nullptr));
		Notify();
	}

	void CommandCloseThenStop()
	{
		SyncOpenFile();

		std::lock_guard<std::mutex> guard(m_cmdQueueAccessMutex);
		m_cmdQueue.push_back(CommandPayload(Command::CMD_CLOSE_THEN_STOP, nullptr));
		Notify();
	}

	bool IsDecoding() const
	{
		std::lock_guard<std::mutex> guard(m_cmdQueueAccessMutex);
		return m_currCmd == Command::CMD_DECODE;
	}
	
	void WaitForNotify()
	{
		int newCount;
		{
			std::lock_guard<std::mutex> lock(m_semaphoreCountMutex);
			m_semaphoreCount--;

			newCount = m_semaphoreCount;
		}

		if (newCount < 0)
		{
			std::unique_lock<std::mutex> newCmdLock(m_cmdCondMutex);
			m_cmdCond.wait(newCmdLock);
		}
		
	}

	void Notify()
	{
		int oldCount;
		{
			std::lock_guard<std::mutex> lock(m_semaphoreCountMutex);
			oldCount = m_semaphoreCount;
			m_semaphoreCount++;
		}

		if (oldCount < 0)
		{
			m_cmdCond.notify_one();
		}
	}

	virtual uint32 Run() override
	{
		while (m_running)
		{
			// Get the next command
			{
				std::lock_guard<std::mutex> guard(m_cmdQueueAccessMutex);
				if (!m_cmdQueue.empty())
				{
					CommandPayload& commandPayload = m_cmdQueue.front();
					m_currCmd = commandPayload.cmd;
					commandPayload.RunPayload();

					m_cmdQueue.pop_front();
				}
			}

			// Wait special command NOP
			if (m_currCmd == Command::CMD_NOP)
			{
				WaitForNotify();
			}

			switch (m_currCmd)
			{
			case Command::CMD_OPEN:
			{
				if (ProcessOpenFile())
				{
					m_openingCompletedCallback(m_videoFormatCtx->duration, m_videoFrameRate, m_videoCodecParam->width, m_videoCodecParam->height);
					m_openingPromise.set_value();
					// Switch to NOP, need explicitly call command decode to commence
					std::lock_guard<std::mutex> guard(m_cmdQueueAccessMutex);
					m_cmdQueue.push_back(CommandPayload(Command::CMD_NOP, nullptr));
				}
				else
				{
					m_openingPromise.set_value(); // keep promise
					m_running = false;
				}

				break;
			}

			case Command::CMD_DECODE:
			{
				bool isFull = m_checkBufferFullCallback();
				m_bufferFullCallback(isFull);

				if (!isFull)
				{
					int decodeResult = ProcessDecoding();
					if (decodeResult == 0)
					{
						m_running = false;
					}
					else if (decodeResult == 2) // EOF
					{
						// EOF, switch to idle
						std::lock_guard<std::mutex> guard(m_cmdQueueAccessMutex);
						m_cmdQueue.push_back(CommandPayload(Command::CMD_NOP, nullptr));
					}
				}
				else
				{
					// Buffer full, switch to idle
					std::lock_guard<std::mutex> guard(m_cmdQueueAccessMutex);
					m_cmdQueue.push_back(CommandPayload(Command::CMD_NOP, nullptr));
				}

				break;
			}

			case Command::CMD_SEEK:
			{
				if (ProcessSeek())
				{
					// switch to continuous decode
					std::lock_guard<std::mutex> guard(m_cmdQueueAccessMutex);
					m_cmdQueue.push_back(CommandPayload(Command::CMD_DECODE, nullptr));
				}
				else
					m_running = false;
				break;
			}
			case Command::CMD_CLOSE:
			{
				ProcessCloseFile();
				std::lock_guard<std::mutex> guard(m_cmdQueueAccessMutex);
				m_cmdQueue.push_back(CommandPayload(Command::CMD_NOP, nullptr));
				break;
			}
			case Command::CMD_CLOSE_THEN_STOP:
			{
				ProcessCloseFile();
				m_running = false;
				break;
			}

			}

			
		}

		return 0;
	}

	virtual void Stop() override
	{
		CommandCloseThenStop();
	}

	virtual void Exit() override
	{
		// nothing to do

	}

private:


	int64_t Timestamp2Framenumber(double timestamp) const
	{
		return (int64_t)(timestamp * m_videoTimebase.den / m_videoTimebase.num);
	}

	double Framenumber2Timestamp(int64_t index) const
	{
		return (double)(index * m_videoTimebase.num) / m_videoTimebase.den;
	}

	void SyncOpenFile()
	{
		if (m_openingFuture.valid())
			m_openingFuture.wait();
	}

	static int OnAVIOReadPacket(void* opaque, uint8_t* buf, int buf_size)
	{
		FFFmpegDecodingThread* thread = (FFFmpegDecodingThread*)opaque;
		auto fileStream = thread->m_fileStream;
		int64_t before_pos = fileStream->Tell();
		int64_t total_size = fileStream->TotalSize();
		if (before_pos + buf_size > total_size)
		{
			buf_size = total_size - before_pos;
		}
		
		if (buf_size > 0)
		{
			fileStream->Serialize(buf, buf_size);
			// This check should be redundant now but keep it safe here
			if (fileStream->GetError())
			{
				// Might read across EOF, try again
				fileStream->ClearError();

				fileStream->Serialize(buf, total_size - before_pos);
				if (!fileStream->GetError())
				{
					return (int)(total_size - before_pos);
				}
				else
				{
					fileStream->ClearError();
					return 0;
				}
			}
			int64_t after_pos = fileStream->Tell();
			return (int)(after_pos - before_pos);
		}
		else
		{
			return 0;
		}
	}

	static int64_t OnAVIOSeek(void* opaque, int64_t offset, int origin)
	{
		FFFmpegDecodingThread* thread = (FFFmpegDecodingThread*)opaque;
		auto fileStream = thread->m_fileStream;
		int64_t size = fileStream->TotalSize();

		if (origin == 0) // SEEK_SET
		{
			fileStream->Seek(std::max((int64_t)0, std::min(offset, size)));
			return fileStream->Tell();
		}
		else if (origin == 1) // SEEK_CUR
		{
			int64_t curr = fileStream->Tell();
			fileStream->Seek(std::max((int64_t)0, std::min(curr + offset, size)));
			return fileStream->Tell();
		}
		else if (origin == 2) // SEEK_END
		{
			fileStream->Seek(std::max((int64_t)0, std::min(size + offset, size)));
			return fileStream->Tell();
		}
		else if (origin == 0x10000)
		{
			return size;
		}
		return 0;
	}

	bool ProcessOpenFile()
	{
		m_videoFormatCtx = avformat_alloc_context();

		// Check if local or network url
		if (strncmp(m_openFilename, "http", 4) == 0)
		{
			if (avformat_open_input(&m_videoFormatCtx, m_openFilename, NULL, NULL) != 0) {
				UE_LOG(EvercoastReaderLog, Error, TEXT("Cannot open the file: %s"), ANSI_TO_TCHAR(m_openFilename));
				return false;
			}

		}
		else
		{
			// Custom IO using Unreal's FArchive interface
			FArchive* ar = IFileManager::Get().CreateFileReader(ANSI_TO_TCHAR(m_openFilename));
			if (ar)
			{
				m_fileStream = MakeShareable(ar);
				if (!m_fileStream->GetError() && m_fileStream->IsLoading())
				{
					UE_LOG(EvercoastReaderLog, Verbose, TEXT("Open file successful: %s"), ANSI_TO_TCHAR(m_openFilename));
				}
				else
				{

					UE_LOG(EvercoastReaderLog, Error, TEXT("Open file with error: %s"), ANSI_TO_TCHAR(m_openFilename));
					m_fileStream->ClearError();
					return false;
				}
			}
			else
			{
				UE_LOG(EvercoastReaderLog, Error, TEXT("Open failed: %s"), ANSI_TO_TCHAR(m_openFilename));
				return false;
			}

			int io_buffer_size = 1024 * 1024;
			m_ioBuffer = av_malloc(io_buffer_size);


			m_videoIOCtx = avio_alloc_context((unsigned char*)m_ioBuffer, io_buffer_size, 0, this, OnAVIOReadPacket, nullptr, OnAVIOSeek);

			// Assign custom io context to format context
			m_videoFormatCtx->pb = m_videoIOCtx;

			if (avformat_open_input(&m_videoFormatCtx, "UnrealFArchive", NULL, NULL) != 0) {
				UE_LOG(EvercoastReaderLog, Error, TEXT("Cannot open the file: %s"), ANSI_TO_TCHAR(m_openFilename));
				return false;
			}
		}


		UE_LOG(EvercoastReaderLog, Log, TEXT("Format: %s"), *FString(m_videoFormatCtx->iformat->name));

		if (avformat_find_stream_info(m_videoFormatCtx, NULL) < 0) {
			UE_LOG(EvercoastReaderLog, Error, TEXT("Could not get the stream info"));
			return false;
		}

		const AVCodec* pCodec = NULL;
		// this component describes the properties of a codec used by the stream i
		AVCodecParameters* pCodecParam = NULL;

		// loop though all the streams and print its main information
		for (unsigned int i = 0; i < m_videoFormatCtx->nb_streams; i++)
		{
			const AVStream& avStream = *m_videoFormatCtx->streams[i];

			AVCodecParameters* pLocalCodecParameters = NULL;
			pLocalCodecParameters = avStream.codecpar;


			UE_LOG(EvercoastReaderLog, Log, TEXT("AVStream->time_base before open coded %d/%d"), avStream.time_base.num, avStream.time_base.den);
			UE_LOG(EvercoastReaderLog, Log, TEXT("AVStream->r_frame_rate before open coded %d/%d"), avStream.r_frame_rate.num, avStream.r_frame_rate.den);
			UE_LOG(EvercoastReaderLog, Log, TEXT("AVStream->start_time %" PRId64), avStream.start_time);
			UE_LOG(EvercoastReaderLog, Log, TEXT("AVStream->duration %" PRId64 " in seconds: %.3f"), avStream.duration, (double)avStream.duration * avStream.time_base.num / avStream.time_base.den);

			UE_LOG(EvercoastReaderLog, Log, TEXT("Finding the proper decoder (CODEC)"));

			// finds the registered decoder for a codec ID
			const AVCodec* pLocalCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id);

			if (pLocalCodec == NULL) {
				UE_LOG(EvercoastReaderLog, Error, TEXT("Unsupported codec!"));
				// In this example if the codec is not found we just skip it
				continue;
			}

			// when the stream is a video we store its index, codec parameters and codec
			if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
				// select whatever the first video stream
				if (m_videoStreamIndex == -1) {
					m_videoStreamIndex = i;
					m_videoCodec = pLocalCodec;
					m_videoCodecParam = pLocalCodecParameters;

					// Hopefully we get a proper frame rate
					m_videoFrameRate = (int32_t)((float)avStream.r_frame_rate.num / (float)avStream.r_frame_rate.den + 0.5f);
					m_videoTimebase = avStream.time_base;
				}

				UE_LOG(EvercoastReaderLog, Log, TEXT("Video Codec: resolution %d x %d"), pLocalCodecParameters->width, pLocalCodecParameters->height);
			}
			else if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
				UE_LOG(EvercoastReaderLog, Log, TEXT("Audio Codec: %d channels, sample rate %d"), pLocalCodecParameters->ch_layout.nb_channels, pLocalCodecParameters->sample_rate);
			}

			// print its name, id and bitrate
			UE_LOG(EvercoastReaderLog, Log, TEXT("Codec %s ID %d bit_rate %lld"), *FString(pLocalCodec->name), pLocalCodec->id, pLocalCodecParameters->bit_rate);

		}

		if (!m_videoCodec || !m_videoCodecParam)
		{
			UE_LOG(EvercoastReaderLog, Error, TEXT("No codec or code param can be retrieved! Cannot decode video."));
			return false;
		}

		if (m_videoStreamIndex == -1)
		{
			UE_LOG(EvercoastReaderLog, Error, TEXT("No video stream found! Cannot decode video."));
			return false;
		}

		// Make codec context
		m_videoCodecCtx = avcodec_alloc_context3(m_videoCodec);
		if (!m_videoCodecCtx)
		{
			return false;
		}

		// Fill the codec context based on the values from the supplied codec parameters
		if (avcodec_parameters_to_context(m_videoCodecCtx, m_videoCodecParam) < 0)
		{
			return false;
		}

		m_videoCodecCtx->thread_count = FGenericPlatformMisc::NumberOfCoresIncludingHyperthreads();
		m_videoCodecCtx->thread_type = FF_THREAD_SLICE;

		// Initialize the AVCodecContext to use the given AVCodec.
		// https://ffmpeg.org/doxygen/trunk/group__lavc__core.html#ga11f785a188d7d9df71621001465b0f1d
		if (avcodec_open2(m_videoCodecCtx, m_videoCodec, NULL) < 0)
		{
			return false;
		}

		// https://ffmpeg.org/doxygen/trunk/structAVFrame.html
		m_currVideoFrame = av_frame_alloc();
		if (!m_currVideoFrame)
		{
			return false;
		}
		// https://ffmpeg.org/doxygen/trunk/structAVPacket.html
		m_currVideoPacket = av_packet_alloc();
		if (!m_currVideoPacket)
		{
			return false;
		}

		return true;
	}

	int ProcessDecoding()
	{
		int response = -1;
		while ((response = av_read_frame(m_videoFormatCtx, m_currVideoPacket)) >= 0)
		{
			// if it's the video stream
			if (m_currVideoPacket->stream_index == m_videoStreamIndex) {
				//UE_LOG(EvercoastReaderLog, Log, TEXT("AVPacket->pts %" PRId64), m_currVideoPacket->pts);

				const AVStream& avStream = *m_videoFormatCtx->streams[m_videoStreamIndex];
				//UE_LOG(EvercoastReaderLog, Log, TEXT("DecodePacket on stream %d"), m_videoStreamIndex);
				response = DecodePacket(&avStream, m_currVideoPacket, m_videoCodecCtx, m_currVideoFrame);
			}
			av_packet_unref(m_currVideoPacket);
			break;
		}

		if (response == AVERROR_EOF)
		{
			UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("EOF reached!"));
			m_reachedStreamEndCallback(m_lastHoggedFrameIndex);

			using namespace std::chrono_literals;
			std::this_thread::sleep_for(1ms);
			// EOF is not an error
			return 2;
		}
		else if (response != 0) // other issue
		{
			char buf[AV_ERROR_MAX_STRING_SIZE];
			av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, response);
			UE_LOG(EvercoastReaderLog, Error, TEXT("ProcessDecoding result in error: %s(%d)"), ANSI_TO_TCHAR(buf), response);
		}
		return (int)(response == 0);
	}

	int DecodePacket(const AVStream* pStream, AVPacket* pPacket, AVCodecContext* pCodecContext, AVFrame* pFrame)
	{
		// Supply raw packet data as input to a decoder
		// https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga58bc4bf1e0ac59e27362597e467efff3
		int response = avcodec_send_packet(pCodecContext, pPacket);

		char error_msg[AV_ERROR_MAX_STRING_SIZE];
		if (response < 0) {
			av_make_error_string(error_msg, AV_ERROR_MAX_STRING_SIZE, response);
			UE_LOG(EvercoastReaderLog, Error, TEXT("Error while sending a packet to the decoder: %s(%d)"), ANSI_TO_TCHAR(error_msg), response);
			return response;
		}

		while (response >= 0)
		{
			// Return decoded output data (into a frame) from a decoder
			// https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga11e6542c4e66d3028668788a1a74217c
			response = avcodec_receive_frame(pCodecContext, pFrame);
			if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
				av_make_error_string(error_msg, AV_ERROR_MAX_STRING_SIZE, response);
				UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("%s(%d)"), ANSI_TO_TCHAR(error_msg), response);
				break;
			}
			else if (response < 0) {
				av_make_error_string(error_msg, AV_ERROR_MAX_STRING_SIZE, response);
				UE_LOG(EvercoastReaderLog, Error, TEXT("Error while receiving a frame from the decoder: %s(%d)"), ANSI_TO_TCHAR(error_msg), response);
				return response;
			}

			if (response >= 0)
			{
				// Check if the frame is a planar YUV 4:2:0, 12bpp
				// That is the format of the provided .mp4 file
				// RGB formats will definitely not give a gray image
				// Other YUV image may do so, but untested, so give a warning
				if (pFrame->format != AV_PIX_FMT_YUV420P && pFrame->format != AV_PIX_FMT_YUVJ420P)
				{
					UE_LOG(EvercoastReaderLog, Warning, TEXT("Warning: colour format is not planar YUV420"));
				}

				// Y:U:V = 1:2:2
				check(pFrame->linesize[0] == pFrame->linesize[1] * 2 && pFrame->linesize[0] == pFrame->linesize[2] * 2);

				// All ownerships to be transfered to texture update lambda
				uint32_t YPitch = sizeof(uint8_t) * pFrame->linesize[0];
				uint32_t UPitch = sizeof(uint8_t) * pFrame->linesize[1];
				uint32_t VPitch = sizeof(uint8_t) * pFrame->linesize[2];

				double frameTimestamp = (double)(pFrame->pts * pStream->time_base.num) / pStream->time_base.den;
				int64_t frameIndex = pFrame->pts / pPacket->duration;

				UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Decoded frame=%" PRId64 ", time=%.2f"), frameIndex, frameTimestamp);
				m_convertTextureCallback(frameTimestamp, frameIndex, pFrame->pts, pFrame->width, pFrame->height, YPitch, UPitch, VPitch,
					pFrame->data[0], pFrame->data[1], pFrame->data[2]);

				m_lastHoggedFramePts = pFrame->pts;
				m_lastHoggedFrameIndex = frameIndex;
			}
		}
		return 0;
	}

	bool ProcessSeek()
	{
		if (av_seek_frame(m_videoFormatCtx, m_videoStreamIndex, Timestamp2Framenumber(m_seekTargetTimestamp), AVSEEK_FLAG_BACKWARD) < 0)
		{
			UE_LOG(EvercoastReaderLog, Error, TEXT("Error seeking to timestamp: %.2f, frame: %lld"), m_seekTargetTimestamp, Timestamp2Framenumber(m_seekTargetTimestamp));
			return false;
		}

		UE_LOG(EvercoastReaderLog, Verbose, TEXT("Video seeked to timestamp: %.2f"), m_seekTargetTimestamp);

		avcodec_flush_buffers(m_videoCodecCtx);

		if (m_currVideoPacket)
		{
			av_packet_free(&m_currVideoPacket);
			m_currVideoPacket = av_packet_alloc();
		}
		if (m_currVideoFrame)
		{
			av_frame_free(&m_currVideoFrame);
			m_currVideoFrame = av_frame_alloc();
		}

		m_seekCompletedCallback();
		return true;
	}

	void ProcessCloseFile()
	{
		if (m_currVideoPacket)
		{
			av_packet_free(&m_currVideoPacket);
			m_currVideoPacket = nullptr;
		}
		if (m_currVideoFrame)
		{
			av_frame_free(&m_currVideoFrame);
			m_currVideoFrame = nullptr;
		}
		if (m_videoCodecCtx)
		{
			avcodec_free_context(&m_videoCodecCtx);
			m_videoCodecCtx = nullptr;
		}

		

		if (m_videoIOCtx)
		{
			// http://ffmpeg.org/doxygen/2.5/avio_8h.html
			// Memory block for input/output operations via AVIOContext. 
			// The buffer must be allocated with av_malloc() and friends. 
			// It may be freed and replaced with a new buffer by libavformat. 
			// AVIOContext.buffer holds the buffer currently in use, which must be later freed with av_free().
			if (m_ioBuffer)
			{
				// Note just releasing m_ioBuffer will likely cause access violation, see comments above.
				av_free(m_videoIOCtx->buffer);
				m_ioBuffer = nullptr;
			}

			avio_context_free(&m_videoIOCtx);
			m_videoIOCtx = nullptr;
		}

		if (m_videoFormatCtx)
		{
			avformat_close_input(&m_videoFormatCtx);
			m_videoFormatCtx = nullptr;
		}


		m_videoCodec = nullptr;
		m_videoCodecParam = nullptr;
		m_videoFrameRate = -1;
		m_videoStreamIndex = -1;
		m_seekTargetTimestamp = 0;
		m_lastHoggedFramePts = 0;
		m_lastHoggedFrameIndex = -1;
	}

};

UFFmpegVideoTextureHog::UFFmpegVideoTextureHog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	m_videoOpened(false),
	m_videoEndReached(false),
	m_videoEndFrameIndex(-1),
	m_hoggingStoppedDueToFullBuffer(false),

	m_videoFrameRate(-1),
	m_textureBufferStart(0),
	m_textureBufferEnd(0),
	
	m_currSeekingTarget(0),
	m_currSeekingTargetPrecache(0),
	m_currSeekingTargetPostcache(0),
	m_lastQueriedTextureIndex(-1),
	m_videoDuration(0),

#if FORCE_NV12_CPU_CONVERSION
	m_scratchPadRGBA(nullptr),
#endif
	m_scratchPadY(nullptr),
	m_scratchPadU(nullptr),
	m_scratchPadV(nullptr)
{
	m_runnable = new FFFmpegDecodingThread();
	m_runnableController = FRunnableThread::Create(m_runnable, TEXT("Evercoast FFmpeg Decoding Thread"));
}

UFFmpegVideoTextureHog::~UFFmpegVideoTextureHog()
{
    Destroy();
}


void UFFmpegVideoTextureHog::Destroy()
{
	if (m_runnable)
	{
		m_runnable->Stop();
	}

	if (m_runnableController)
		m_runnableController->Kill(true);

	DrainRHICommandList();

	delete m_runnableController;
	m_runnableController = nullptr;

	delete m_runnable;
	m_runnable = nullptr;

	// explicitly free texture here to prevent render thread access violation
	for (int i = 0; i < m_textureBuffer.Num(); ++i)
	{
		m_textureBuffer[i]->FreeTexture();
        m_textureBuffer[i]->RemoveFromRoot();
	}

	m_textureBuffer.Empty();
#if FORCE_NV12_CPU_CONVERSION
	delete[] m_scratchPadRGBA;
	m_scratchPadRGBA = nullptr;
#endif
	delete[] m_scratchPadY;
	m_scratchPadY = nullptr;
	delete[] m_scratchPadU;
	m_scratchPadU = nullptr;
	delete[] m_scratchPadV;
	m_scratchPadV = nullptr;
}


void UFFmpegVideoTextureHog::DrainRHICommandList()
{
	std::promise<void> promise;
	std::future<void> future = promise.get_future();
	ENQUEUE_RENDER_COMMAND(FlushRHIResources)([&promise = promise](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
			promise.set_value();
		});

	future.get();
}

bool UFFmpegVideoTextureHog::OpenFile(const FString& filePath)
{
	m_videoOpened = false;
	m_videoEndReached = false;
	m_videoEndFrameIndex = -1;
	m_textureBufferStart = 0;
	m_textureBufferEnd = 0;
	m_hoggingStoppedDueToFullBuffer = false;

	m_runnable->CommandOpenFile(TCHAR_TO_ANSI(filePath.GetCharArray().GetData()), 
		std::bind(&UFFmpegVideoTextureHog::OnVideoOpened, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
		std::bind(&UFFmpegVideoTextureHog::OnVideoEndReached, this, std::placeholders::_1),
		[this](double timestamp, int64_t frame_index, int64_t frame_pts, int width, int height, 
			uint32_t y_pitch, uint32_t u_pitch, uint32_t v_pitch, uint8_t* y_data, uint8_t* u_data, uint8_t* v_data) 
		{
			OnConvertNV12Texture(timestamp, frame_index, frame_pts, width, height, y_pitch, u_pitch, v_pitch, y_data, u_data, v_data);
		},
		std::bind(&UFFmpegVideoTextureHog::IsFull, this),
		std::bind(&UFFmpegVideoTextureHog::OnFull, this, std::placeholders::_1));

	return true;
}


bool UFFmpegVideoTextureHog::OpenUrl(const FString& url)
{
	return OpenFile(url);
}

bool UFFmpegVideoTextureHog::OpenSource(UMediaSource* source)
{
	return OpenFile(source->GetUrl());
}

void UFFmpegVideoTextureHog::OnVideoOpened(int64_t avformat_duration, int32_t frame_rate, int frame_width, int frame_height)
{
	std::lock_guard<std::recursive_mutex> guard(m_controlBitMutex);
	// save the params and leave to main thread to work on it.
	m_videoOpenParams = {
		true,
		avformat_duration,
		frame_rate,
		frame_width,
		frame_height
	};

	m_currSeekingTarget = 0;
	m_currSeekingTargetPrecache = 0;
	m_currSeekingTargetPostcache = 2.0;
}

bool UFFmpegVideoTextureHog::IsVideoOpened()
{
	return m_videoOpened;
}

bool UFFmpegVideoTextureHog::Close()
{
	m_runnable->CommandClose();
	return true;
}

void UFFmpegVideoTextureHog::OnVideoEndReached(int64_t last_frame_index)
{
	m_videoEndFrameIndex = last_frame_index;
	m_videoEndReached = true;

	std::lock_guard<std::recursive_mutex> guard(m_textureRecordMutex);

	const UTextureRecord* const* ppTR = m_textureBuffer.FindByPredicate([last_frame_index](const UTextureRecord* tr)
		{
			return tr->frameIndex == last_frame_index;
		});

	if (ppTR && *ppTR && m_videoDuration > (*ppTR)->frameTimestamp)
	{
		m_videoDuration = (*ppTR)->frameTimestamp;
	}
}

bool UFFmpegVideoTextureHog::ResetTo(double timestamp, const std::function<void()>& callback)
{
	if (!m_videoOpened)
		return false;

	m_currSeekingTarget = timestamp;

	// Including the 1 sec precache
	m_currSeekingTargetPrecache = timestamp - 1.0;
	if (m_currSeekingTargetPrecache < 0)
		m_currSeekingTargetPrecache = 0;

	m_currSeekingTargetPostcache = m_currSeekingTargetPrecache + 2.0;

	m_runnable->CommandSeekTo(m_currSeekingTargetPrecache,
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
		[=, this]() 
#else
		[=]()
#endif
		{
			m_videoEndReached = false;
			m_videoEndFrameIndex = -1;

			// Should be completed here
			InvalidateAllTextures();

			callback();
		});
	return true;
}

bool UFFmpegVideoTextureHog::StartHogging()
{
	if (!m_videoOpened)
		return false;

	m_runnable->CommandResumeDecoding();
	return true;
}

bool UFFmpegVideoTextureHog::StopHogging()
{
	m_runnable->CommandPauseDecoding();
	return true;
}

bool UFFmpegVideoTextureHog::IsHogging() const
{
	return m_runnable->IsDecoding();
}

bool UFFmpegVideoTextureHog::IsEndReached() const
{
	if (m_videoEndReached)
	{
		std::lock_guard<std::recursive_mutex> guard(m_textureRecordMutex);
		if (m_lastQueriedTextureIndex == m_videoEndFrameIndex || m_textureBufferEnd == m_textureBufferStart)
			return true;

	}
	return false;
}

bool UFFmpegVideoTextureHog::IsFull() const
{
	std::lock_guard<std::recursive_mutex> guard(m_textureRecordMutex);

    if ((m_textureBufferEnd + 1) % RING_QUEUE_SIZE != m_textureBufferStart)
	{
		return false;
	}

	return true;
}
bool UFFmpegVideoTextureHog::IsFrameIndexWithinDuration(int64_t frameIndex) const
{
    if (m_videoOpened)
	{
		int64_t beyondLastFrameIndex = (int64_t)(GetVideoDuration() * m_videoFrameRate);
		if (frameIndex >= 0 && frameIndex < beyondLastFrameIndex)
		{
			return true;
		}
	}

	return false;
}
bool UFFmpegVideoTextureHog::IsHoggingPausedDueToFull() const
{
	std::lock_guard<std::recursive_mutex> guard(m_controlBitMutex);
	return m_hoggingStoppedDueToFullBuffer;
}

void UFFmpegVideoTextureHog::RestartHoggingIfPausedDueToFull()
{
	std::lock_guard<std::recursive_mutex> guard(m_controlBitMutex);

	if (m_hoggingStoppedDueToFullBuffer)
	{
		m_runnable->CommandResumeDecoding();
		m_hoggingStoppedDueToFullBuffer = false;
	}
}

void UFFmpegVideoTextureHog::OnConvertNV12Texture(double timestamp, int64_t frame_index, int64_t frame_pts, int width, int height, uint32_t y_pitch, uint32_t u_pitch, uint32_t v_pitch, uint8_t* y_data, uint8_t* u_data, uint8_t* v_data)
{

	if (!m_scratchPadY)
		m_scratchPadY = new uint8_t[y_pitch * height];

	if (!m_scratchPadU)
		m_scratchPadU = new uint8_t[u_pitch * height / 2];

	if (!m_scratchPadV)
		m_scratchPadV = new uint8_t[v_pitch * height / 2];

	// FIXME: do we really need to make a copy as everything is sync-ed???
	memcpy(m_scratchPadY, y_data, y_pitch * height);
	memcpy(m_scratchPadU, u_data, u_pitch * height / 2);
	memcpy(m_scratchPadV, v_data, v_pitch * height / 2);


	// Scope here to avoid later call to IsFull() being mutex out 
	{
		std::lock_guard<std::recursive_mutex> guard(m_textureRecordMutex);

		UTextureRecord* pOutput = m_textureBuffer[m_textureBufferEnd];

		// Sync this call
		m_renderThreadPromise = std::promise<void>();
		m_renderThreadFuture = m_renderThreadPromise.get_future();

#if FORCE_NV12_CPU_CONVERSION

		UTexture* pTexture = pOutput->texture;

		if (!m_scratchPadRGBA)
		{
			uint32_t RGBADataPitch = sizeof(uint8_t) * 4 * width;
			m_scratchPadRGBA = new uint8_t[RGBADataPitch * height];
		}


		ENQUEUE_RENDER_COMMAND(ConvertNV12TextureOnCPU)(
			[pTexture, y_pitch, u_pitch, v_pitch, y_data=m_scratchPadY, u_data=m_scratchPadU, v_data=m_scratchPadV, rgba_data=m_scratchPadRGBA, width, height, &promise = m_renderThreadPromise]
			(FRHICommandListImmediate& RHICmdList)
			{

				FTextureResource* pRes = pTexture->GetResource();
				check(pRes);
				FRHITexture2D* pRHITex = pRes->GetTexture2DRHI();
				check(pRHITex);

				uint32_t RGBADataPitch = sizeof(uint8_t) * 4 * width; // way to get the proper pitch of RHITexture2D?

				for (int j = 0; j < height; ++j)
				{
					for (int i = 0; i < width; ++i)
					{
						// index thru YUV420 plane
						uint8_t Y = y_data[j * y_pitch + i];
						uint8_t U = u_data[j / 2 * u_pitch + i / 2];
						uint8_t V = v_data[j / 2 * v_pitch + i / 2];

						uint8_t* B = &(rgba_data[j * RGBADataPitch + i * 4]);
						uint8_t* G = &(rgba_data[j * RGBADataPitch + i * 4 + 1]);
						uint8_t* R = &(rgba_data[j * RGBADataPitch + i * 4 + 2]);
						uint8_t* A = &(rgba_data[j * RGBADataPitch + i * 4 + 3]);

						*R = Y + 1.402f * (V - 128);
						*G = Y - 0.344f * (U - 128) - 0.714 * (V - 128);
						*B = Y + 1.772f * (U - 128);
						*A = 255;
					}
				}
				// Decode from CPU YUV420 plane: https://en.wikipedia.org/wiki/YUV#Y%E2%80%B2UV420p_(and_Y%E2%80%B2V12_or_YV12)_to_RGB888_conversion
				// Got to layout the Y/U/V before we issue an update command
				RHIUpdateTexture2D(pRHITex, 0, FUpdateTextureRegion2D(0, 0, 0, 0, width, height), RGBADataPitch, rgba_data);
				promise.set_value();
			});
#else

		
		ENQUEUE_RENDER_COMMAND(ConvertNV12Texture)(
			[YPitch = y_pitch, UPitch = u_pitch, VPitch = v_pitch,
			pYData = y_data, pUData = u_data, pVData = v_data,
			nv12YPlaneRHI = m_nv12YPlaneRHI, nv12UPlaneRHI = m_nv12UPlaneRHI, nv12VPlaneRHI = m_nv12VPlaneRHI,
			width = width, height = height, pTexture = pOutput->texture, &promise = m_renderThreadPromise](FRHICommandListImmediate& RHICmdList)
		{
			RHIUpdateTexture2D(nv12YPlaneRHI, 0, FUpdateTextureRegion2D(0, 0, 0, 0, width, height), YPitch, pYData);
			RHIUpdateTexture2D(nv12UPlaneRHI, 0, FUpdateTextureRegion2D(0, 0, 0, 0, width / 2, height / 2), UPitch, pUData);
			RHIUpdateTexture2D(nv12VPlaneRHI, 0, FUpdateTextureRegion2D(0, 0, 0, 0, width / 2, height / 2), VPitch, pVData);

			RHICmdList.Transition(FRHITransitionInfo(nv12YPlaneRHI, ERHIAccess::Unknown, ERHIAccess::SRVMask));
			RHICmdList.Transition(FRHITransitionInfo(nv12UPlaneRHI, ERHIAccess::Unknown, ERHIAccess::SRVMask));
			RHICmdList.Transition(FRHITransitionInfo(nv12VPlaneRHI, ERHIAccess::Unknown, ERHIAccess::SRVMask));



			FGraphicsPipelineStateInitializer GraphicsPSOInit;

			FTextureRenderTargetResource* pRes = (FTextureRenderTargetResource*)pTexture->GetResource();
			FRHITexture* RenderTarget = pRes->GetTextureRenderTarget2DResource()->GetTextureRHI();

			RHICmdList.Transition(FRHITransitionInfo(RenderTarget, ERHIAccess::SRVMask, ERHIAccess::RTV));

			FRHIRenderPassInfo RPInfo(RenderTarget, ERenderTargetActions::Load_Store);
			RHICmdList.BeginRenderPass(RPInfo, TEXT("FFmpegVideoTextureConversion"));
			{
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				RHICmdList.SetViewport(0, 0, 0.f, width, height, 1.f);

				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
				GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<>::GetRHI();
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;

				FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
				TShaderMapRef<FNV12TextureConversionVS> VertexShader(GlobalShaderMap);
				TShaderMapRef<FNV12TextureConversionPS> PixelShader(GlobalShaderMap);

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GNV12TextureConversionVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

#if ENGINE_MAJOR_VERSION >= 5
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
#else
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
#endif

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
				FShaderResourceViewRHIRef Y_SRV = RHICmdList.CreateShaderResourceView(nv12YPlaneRHI, 
					FRHIViewDesc::CreateTextureSRV()
					.SetDimensionFromTexture(nv12YPlaneRHI)
					.SetMipRange(0, 1)
					.SetFormat(PF_R8));
				FShaderResourceViewRHIRef U_SRV = RHICmdList.CreateShaderResourceView(nv12UPlaneRHI, 
					FRHIViewDesc::CreateTextureSRV()
					.SetDimensionFromTexture(nv12UPlaneRHI)
					.SetMipRange(0, 1)
					.SetFormat(PF_R8));
				FShaderResourceViewRHIRef V_SRV = RHICmdList.CreateShaderResourceView(nv12VPlaneRHI, 
					FRHIViewDesc::CreateTextureSRV()
					.SetDimensionFromTexture(nv12VPlaneRHI)
					.SetMipRange(0, 1)
					.SetFormat(PF_R8));
#else
				FShaderResourceViewRHIRef Y_SRV = RHICreateShaderResourceView(nv12YPlaneRHI, 0, 1, PF_R8);
				FShaderResourceViewRHIRef U_SRV = RHICreateShaderResourceView(nv12UPlaneRHI, 0, 1, PF_R8);
				FShaderResourceViewRHIRef V_SRV = RHICreateShaderResourceView(nv12VPlaneRHI, 0, 1, PF_R8);
#endif

				PixelShader->SetParameters(RHICmdList, Y_SRV, U_SRV, V_SRV);

				RHICmdList.SetStreamSource(0, GNV12TextureConversionVertexBuffer.VertexBufferRHI, 0);
				RHICmdList.SetViewport(0, 0, 0.f, width, height, 1.f);
				RHICmdList.DrawIndexedPrimitive(GNV12TextureConversionIndexBuffer.IndexBufferRHI, 0, 0, 4, 0, 2, 1);
			}
			RHICmdList.EndRenderPass();
			RHICmdList.Transition(FRHITransitionInfo(RenderTarget, ERHIAccess::RTV, ERHIAccess::SRVMask));
			// necessary to dispatch all the commands to avoid corrupted frames
			RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
			// nessary to make sure the commands are dispatched and we return to modify the ring buffer index
			promise.set_value();
		}
		);

		
#endif

		m_renderThreadFuture.get();

		pOutput->SetFrameTimestamp(frame_index, timestamp);
		pOutput->MarkAsUsed(false);

		UE_LOG(EvercoastReaderLog, Verbose, TEXT("Convert frame=%" PRId64 ", time=%.2f, slot=%d"), frame_index, timestamp, m_textureBufferEnd);

		m_textureBufferEnd = (m_textureBufferEnd + 1) % RING_QUEUE_SIZE;
	}

}

void UFFmpegVideoTextureHog::OnFull(bool isFull)
{
	std::lock_guard<std::recursive_mutex> guard1(m_controlBitMutex);
	m_hoggingStoppedDueToFullBuffer = isFull;
}

void UFFmpegVideoTextureHog::Tick(UWorld* world)
{
	std::lock_guard<std::recursive_mutex> guard(m_controlBitMutex);
	// We need to do this because textures are supposed to be initialised on main thread
	if (m_videoOpenParams.isPending)
	{
		m_videoOpened = true;
		m_videoEndReached = false;
		m_hoggingStoppedDueToFullBuffer = false;
		m_videoFrameRate = m_videoOpenParams.frameRate;

		DrainRHICommandList();

		// WHY we need a ring queue that can fit at least 3 second of data ???
		// because the encoded videos are not necessarily 15 or 30 frames give a keyframe so to be safe,
		// instead of cache 1 sec prior and after to the seek target, respectively, we'll need 1.5 sec for each.
		// also, sometimes ffmpeg doesn't always seek to the nearest keyframe for some reason, so we'll need larger pool
		const int SIZE_MULTIPLIER = 3; // <- Might need to change if see worse cases

		if (m_videoFrameRate * SIZE_MULTIPLIER > m_textureBuffer.Num())
		{
			RING_QUEUE_SIZE = (int)(m_videoFrameRate * SIZE_MULTIPLIER + 0.5f);

			m_textureBuffer.SetNumZeroed(RING_QUEUE_SIZE, true);
			for (int i = 0; i < m_textureBuffer.Num(); ++i)
			{
				m_textureBuffer[i] = NewObject<UTextureRecord>(GetTransientPackage());
                m_textureBuffer[i]->AddToRoot();
			}

		}
		for (int i = 0; i < m_textureBuffer.Num(); ++i)
		{
			m_textureBuffer[i]->FreeTexture();
#if FORCE_NV12_CPU_CONVERSION
			m_textureBuffer[i]->InitTexture(m_videoOpenParams.frameWidth, m_videoOpenParams.frameHeight, i);
#else
			m_textureBuffer[i]->InitRenderTargetableTexture(m_videoOpenParams.frameWidth, m_videoOpenParams.frameHeight, i);
#endif
		}

#if !FORCE_NV12_CPU_CONVERSION
		m_renderThreadPromise = std::promise<void>();
		m_renderThreadFuture = m_renderThreadPromise.get_future();

		ENQUEUE_RENDER_COMMAND(InitNV12RHI)([this, frame_width=m_videoOpenParams.frameWidth, frame_height=m_videoOpenParams.frameHeight, &promise = m_renderThreadPromise](FRHICommandListImmediate& RHICmdList)
			{
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION < 2
				FRHIResourceCreateInfo CreateInfo(TEXT("NV12ConversionYUVTexture"));

				m_nv12YPlaneRHI = RHICreateTexture2D(frame_width, frame_height, PF_R8, 1, 1, TexCreate_Dynamic | TexCreate_ShaderResource, CreateInfo);
				m_nv12UPlaneRHI = RHICreateTexture2D(frame_width / 2, frame_height / 2, PF_R8, 1, 1, TexCreate_Dynamic | TexCreate_ShaderResource, CreateInfo);
				m_nv12VPlaneRHI = RHICreateTexture2D(frame_width / 2, frame_height / 2, PF_R8, 1, 1, TexCreate_Dynamic | TexCreate_ShaderResource, CreateInfo);
#else
				const FRHITextureCreateDesc DescY =
					FRHITextureCreateDesc::Create2D(TEXT("NV12ConversionYUVTexture"), frame_width, frame_height, PF_R8)
					.SetNumMips(1)
					.SetFlags(ETextureCreateFlags::Dynamic | ETextureCreateFlags::ShaderResource)
					.SetInitialState(ERHIAccess::SRVMask);

				const FRHITextureCreateDesc DescUV =
					FRHITextureCreateDesc::Create2D(TEXT("NV12ConversionYUVTexture"), frame_width / 2, frame_height / 2, PF_R8)
					.SetNumMips(1)
					.SetFlags(ETextureCreateFlags::Dynamic | ETextureCreateFlags::ShaderResource)
					.SetInitialState(ERHIAccess::SRVMask);

				m_nv12YPlaneRHI = RHICreateTexture(DescY);
				m_nv12UPlaneRHI = RHICreateTexture(DescUV);
				m_nv12VPlaneRHI = RHICreateTexture(DescUV);
#endif
#else
				FRHIResourceCreateInfo CreateInfo;
				m_nv12YPlaneRHI = RHICreateTexture2D(frame_width, frame_height, PF_R8, 1, 1, TexCreate_Dynamic | TexCreate_ShaderResource, CreateInfo);
				m_nv12UPlaneRHI = RHICreateTexture2D(frame_width / 2, frame_height / 2, PF_R8, 1, 1, TexCreate_Dynamic | TexCreate_ShaderResource, CreateInfo);
				m_nv12VPlaneRHI = RHICreateTexture2D(frame_width / 2, frame_height / 2, PF_R8, 1, 1, TexCreate_Dynamic | TexCreate_ShaderResource, CreateInfo);
#endif
				

				RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
				promise.set_value();
			});

		m_renderThreadFuture.get();
#endif

		m_textureBufferStart = 0;
		m_textureBufferEnd = 0;
		m_videoDuration = m_videoOpenParams.avformatDuration * (1.0 / AV_TIME_BASE);

#if FORCE_NV12_CPU_CONVERSION
		delete[] m_scratchPadRGBA;
		m_scratchPadRGBA = nullptr;
#endif
		delete[] m_scratchPadY;
		m_scratchPadY = nullptr;
		delete[] m_scratchPadU;
		m_scratchPadU = nullptr;
		delete[] m_scratchPadV;
		m_scratchPadV = nullptr;


		StartHogging();

		m_videoOpenParams = VideoOpenParams();
	}
}

float UFFmpegVideoTextureHog::GetVideoDuration() const
{
	if (m_videoOpened)
		return m_videoDuration;

	// Make sure you get a wrong duration if the video hasn't been properly opened
	return 0.f;
}

UTexture* UFFmpegVideoTextureHog::QueryTextureAtIndex(int64_t frameIndex) const
{
	std::lock_guard<std::recursive_mutex> guard(m_textureRecordMutex);

	const UTextureRecord *const * ppTR = m_textureBuffer.FindByPredicate([frameIndex](const UTextureRecord* tr)
		{
			return tr->frameIndex == frameIndex && !tr->isUsed;
		});

	if (ppTR)
	{
		m_lastQueriedTextureIndex = frameIndex;
		return (*ppTR)->texture;
	}

	return nullptr;
}
// mark textures from the start till the requested texture as invalid
bool UFFmpegVideoTextureHog::InvalidateTextureAndBefore(UTexture* pTex)
{
	std::lock_guard<std::recursive_mutex> guard(m_textureRecordMutex);

    int32 IndexFound = m_textureBuffer.IndexOfByPredicate([pTex](const UTextureRecord* tr)
		{
			return tr->texture == pTex;
		}
	);

	if (IndexFound != INDEX_NONE)
	{
		m_textureBuffer[IndexFound]->MarkAsUsed(true);

		// shall we remove all the texture behind this one, till the start of the ring queue
		for (int i = m_textureBufferStart; i != IndexFound; )
		{
			m_textureBuffer[i]->MarkAsUsed(true);
			i = (i + 1) % RING_QUEUE_SIZE;
		}

		// a new start
		m_textureBufferStart = (IndexFound + 1) % RING_QUEUE_SIZE;

		// If we detect the hogging was stopped because the buffer is full, we should
		// resume hogging here, after freeing some slots
		RestartHoggingIfPausedDueToFull();
		return true;
	}
	return false;
}
// mark textures from the start till the texture which has the requested frame index as invalid
bool UFFmpegVideoTextureHog::InvalidateTextureAndBeforeByFrameIndex(int64_t frameIndex)
{
	std::lock_guard<std::recursive_mutex> guard(m_textureRecordMutex);

    bool hasFound = false;
	for (int i = 0; i < m_textureBuffer.Num(); ++i)
	{
		auto& record = m_textureBuffer[i];
		if (record->frameIndex <= frameIndex && !record->isUsed)
		{
			hasFound = true;
			record->MarkAsUsed(true);
			m_textureBufferStart = m_textureBufferStart > ((i + 1) % RING_QUEUE_SIZE) ? m_textureBufferStart : ((i + 1) % RING_QUEUE_SIZE);
		}
	}

	RestartHoggingIfPausedDueToFull();
	return hasFound;
}
// mark all textures as invalid
void UFFmpegVideoTextureHog::InvalidateAllTextures()
{
	std::lock_guard<std::recursive_mutex> guard(m_textureRecordMutex);

    for (int i = 0; i < m_textureBuffer.Num(); ++i)
	{
		m_textureBuffer[i]->SetFrameTimestamp(-1, -1);
		m_textureBuffer[i]->MarkAsUsed(false);
	}

	m_textureBufferStart = 0;
	m_textureBufferEnd = 0;
	RestartHoggingIfPausedDueToFull();
}
// -----|<--- f --->|-----
bool UFFmpegVideoTextureHog::IsFrameWithinCachedRange(int64_t frameIndex) const
{
	std::lock_guard<std::recursive_mutex> guard(m_textureRecordMutex);

    int64_t rangeStart = std::numeric_limits<int64>::max();
	int64_t rangeEnd = -1;
	for (int i = m_textureBufferStart; i != m_textureBufferEnd; )
	{
		if (!m_textureBuffer[i]->isUsed)
		{
			if (rangeStart > m_textureBuffer[i]->frameIndex)
			{
				rangeStart = m_textureBuffer[i]->frameIndex;
			}

			if (rangeEnd < m_textureBuffer[i]->frameIndex)
			{
				rangeEnd = m_textureBuffer[i]->frameIndex;
			}
		}
		i = ( i + 1 ) % RING_QUEUE_SIZE;
	}

	return frameIndex >= rangeStart && frameIndex <= rangeEnd;
}
// -----|<--------->|--f--
bool UFFmpegVideoTextureHog::IsFrameBeyondCachedRange(int64_t frameIndex) const
{
	std::lock_guard<std::recursive_mutex> guard(m_textureRecordMutex);

    int64_t rangeStart = std::numeric_limits<int64>::max();
	int64_t rangeEnd = -1;
	for (int i = m_textureBufferStart; i != m_textureBufferEnd; )
	{
		if (!m_textureBuffer[i]->isUsed)
		{
			if (rangeStart > m_textureBuffer[i]->frameIndex)
			{
				rangeStart = m_textureBuffer[i]->frameIndex;
			}

			if (rangeEnd < m_textureBuffer[i]->frameIndex)
			{
				rangeEnd = m_textureBuffer[i]->frameIndex;
			}
		}
		i = (i + 1) % RING_QUEUE_SIZE;
	}

	return frameIndex > rangeStart && frameIndex > rangeEnd;
}

// --f--|<--------->|-----
bool UFFmpegVideoTextureHog::IsFrameBeforeCachedRange(int64_t frameIndex) const
{
	std::lock_guard<std::recursive_mutex> guard(m_textureRecordMutex);

    int64_t rangeStart = std::numeric_limits<int64>::max();
	int64_t rangeEnd = -1;
	for (int i = m_textureBufferStart; i != m_textureBufferEnd; )
	{
		if (!m_textureBuffer[i]->isUsed)
		{
			if (rangeStart > m_textureBuffer[i]->frameIndex)
			{
				rangeStart = m_textureBuffer[i]->frameIndex;
			}

			if (rangeEnd < m_textureBuffer[i]->frameIndex)
			{
				rangeEnd = m_textureBuffer[i]->frameIndex;
			}
		}
		i = (i + 1) % RING_QUEUE_SIZE;
	}

	return frameIndex < rangeStart && frameIndex < rangeEnd;
}

void UFFmpegVideoTextureHog::TrimCache(double medianTimestamp, double halfFrameInterval)
{
	std::lock_guard<std::recursive_mutex> guard(m_textureRecordMutex);

	
	int medianIndex = -1;
	for (int i = m_textureBufferStart; i != m_textureBufferEnd; i = (i + 1) % RING_QUEUE_SIZE)
	{
		auto& record = m_textureBuffer[i];
		if (abs(record->frameTimestamp - medianTimestamp) <= halfFrameInterval)
		{
			medianIndex = i;
			break;
		}
	}

	int halfCacheWidth = RING_QUEUE_SIZE / 2;

	bool hasTrimmed = false;
	if (medianIndex >= 0)
	{
		int medianDistanceToHead;

		// trim head
		if (m_textureBufferStart < medianIndex)
		{
			if (medianIndex - m_textureBufferStart > halfCacheWidth)
			{
				m_textureBufferStart = medianIndex - halfCacheWidth;
				hasTrimmed = true;
			}

			medianDistanceToHead = medianIndex - m_textureBufferStart;
		}
		else if (medianIndex < m_textureBufferStart)
		{
			if (medianIndex + RING_QUEUE_SIZE - m_textureBufferStart > halfCacheWidth)
			{
				m_textureBufferStart = (medianIndex + RING_QUEUE_SIZE - halfCacheWidth) % RING_QUEUE_SIZE;
				hasTrimmed = true;
			}

			medianDistanceToHead = medianIndex + RING_QUEUE_SIZE - m_textureBufferStart;
		}
		else
		{
			// median == start, no trimming
			medianDistanceToHead = 0;
		}

		// trim tail
		if (medianDistanceToHead >= halfCacheWidth) // only consider trim tail when head<->median has grown to size
		{
			if (medianIndex < m_textureBufferEnd)
			{
				if (m_textureBufferEnd - medianIndex > halfCacheWidth)
				{
					m_textureBufferEnd = medianIndex + halfCacheWidth;
					hasTrimmed = true;
				}
			}
			else if (medianIndex > m_textureBufferEnd)
			{
				if (m_textureBufferEnd + RING_QUEUE_SIZE - medianIndex > halfCacheWidth)
				{
					m_textureBufferEnd = (medianIndex + halfCacheWidth - RING_QUEUE_SIZE) % RING_QUEUE_SIZE;
					hasTrimmed = true;
				}
			}
			else
			{
				// median == end, no trimming
			}
		}

		
	}
	else if (m_textureBuffer.Num() > 0)
	{
		// check if the median timestamp is totally running out of cache range
		if (!(m_currSeekingTargetPrecache <= medianTimestamp && medianTimestamp <= m_currSeekingTargetPostcache))
		{
			int lastElementIdx = m_textureBufferEnd - 1;
			if (lastElementIdx < 0)
			{
				lastElementIdx += RING_QUEUE_SIZE;
			}

			if (!m_textureBuffer[lastElementIdx] || medianTimestamp > m_textureBuffer[lastElementIdx]->frameTimestamp)
			{
				// reset
				InvalidateAllTextures();

				UE_LOG(EvercoastReaderLog, Verbose, TEXT("Video Trimmed Diposed"));
				hasTrimmed = true;
			}
		}
	}

	if (hasTrimmed)
	{
		m_currSeekingTargetPrecache = medianTimestamp - 1.0;
		if (m_currSeekingTargetPrecache < 0)
			m_currSeekingTargetPrecache = 0;
		m_currSeekingTarget = medianTimestamp;
		m_currSeekingTargetPostcache = m_currSeekingTargetPrecache + 2.0;

		RestartHoggingIfPausedDueToFull();

		UE_LOG(EvercoastReaderLog, VeryVerbose, TEXT("Video Trimmed: median: %.2f head(%d): %.2f -> tail(%d): %.2f"), medianTimestamp, m_textureBufferStart, m_textureBuffer[m_textureBufferStart]->frameTimestamp, m_textureBufferEnd, m_textureBufferEnd == 0 ? m_textureBuffer[RING_QUEUE_SIZE - 1]->frameTimestamp : m_textureBuffer[m_textureBufferEnd - 1]->frameTimestamp);
	}
}


bool UFFmpegVideoTextureHog::IsTextureBeyondRange(double timestamp, double halfFrameInterval) const
{
	std::lock_guard<std::recursive_mutex> guard(m_textureRecordMutex);

	if (m_textureBufferEnd == m_textureBufferStart)
		return true; // empty

	int lastElementIdx = m_textureBufferEnd - 1;
	if (lastElementIdx < 0)
	{
		lastElementIdx += RING_QUEUE_SIZE;
	}

	if (m_textureBuffer[lastElementIdx] && m_textureBuffer[lastElementIdx]->frameTimestamp + halfFrameInterval < timestamp)
	{
		return true;
	}

	return false;
}