#include "EvercoastDecoder.h"

DEFINE_LOG_CATEGORY(EvercoastDecoderLog);

extern bool g_SupportDecodeVoxel;

std::shared_ptr<EvercoastDecoder> EvercoastDecoder::Create()
{
	if (g_SupportDecodeVoxel)
	{
		return std::shared_ptr<EvercoastDecoder>(new EvercoastDecoder(create_decoder_instance()));
	}
	else
	{
		return std::shared_ptr<EvercoastDecoder>(new EvercoastDecoder(0));
	}
}

EvercoastDecoder::~EvercoastDecoder()
{
	if (m_interface)
	{
		release_decoder_instance(m_interface);
		m_interface = InvalidHandle;
	}
}

static Definition s_InvalidDefinition = {
	0, false, false, false, false, false, false, 0.0
};

Definition EvercoastDecoder::GetDefaultDefinition()
{
	if (m_interface)
		return decoder_default_decode_definition(m_interface);

	return s_InvalidDefinition;
}

DecoderType EvercoastDecoder::GetType() const
{
	return DT_EvercoastVoxel;
}

bool EvercoastDecoder::DecodeMemoryStream(const uint8_t* stream, size_t stream_size, double timestamp, int64_t frameIndex, GenericDecodeOption* option)
{
	if (!m_interface)
		return false;

	if (!decoder_open(m_interface, reinterpret_cast<uintptr_t>(stream), stream_size))
	{
		UE_LOG(EvercoastDecoderLog, Warning, TEXT("Decode failed. Cannot open memory stream"));
		return false;
	}

	EvercoastDecodeOption* evercoastOption = static_cast<EvercoastDecodeOption*>(option);

	if (!decoder_decode(m_interface, evercoastOption->definition))
	{
		UE_LOG(EvercoastDecoderLog, Warning, TEXT("Decode failed. Cannot decode"));
		return false;
	}

    GTHandle voxelFrame = decoder_get_voxel_frame(m_interface);
	if (voxelFrame == InvalidHandle)
	{
		UE_LOG(EvercoastDecoderLog, Warning, TEXT("Decode failed. Cannot get the voxel frame"));
		return false;
	}

	VoxelFrameDefinition frameDef;
	if (!voxel_frame_get_definition(voxelFrame, &frameDef))
	{
		UE_LOG(EvercoastDecoderLog, Warning, TEXT("Decode failed. Cannot extract frame definition"));
		return false;
	}

	if (frameDef.voxel_count > DECODER_MAX_VOXEL_COUNT)
	{
		UE_LOG(EvercoastDecoderLog, Warning, TEXT("Decode failed. Frame exceeeded maximum voxel count"));
		return false;
	}

	UE_LOG(EvercoastDecoderLog, Verbose, TEXT("Decode successful: frame %d, voxel count: %d"), frameDef.frame_number, frameDef.voxel_count);

	m_result = std::make_shared<EvercoastDecodeResult>(true, timestamp, frameIndex, voxelFrame);
	return true;
}

std::shared_ptr<GenericDecodeResult> EvercoastDecoder::GetResult()
{
	return std::move(m_result);
}

EvercoastDecoder::EvercoastDecoder(GTHandle decoder_interface) :
	m_interface(decoder_interface)
{
}
