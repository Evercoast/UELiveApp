#pragma once

#include <memory>
#include "CoreMinimal.h"
#include "UnrealEngineCompatibility.h"
#include "GenericDecoder.h"


#pragma pack(push, 1)
struct ECSpzHeader
{
    uint32_t magic = 0x50534345;
    uint32_t version = 1;
    uint32_t pointCount = 0;
    uint32_t frameNumber = 0;
    uint8_t shDegree = 0;
    uint8_t fractionalBits = 0;
    uint8_t flags = 0;
    uint8_t reserved = 0;
};

#pragma pack(pop)

class EvercoastGaussianSplatDecodeOption : public GenericDecodeOption
{
public:
    EvercoastGaussianSplatDecodeOption(bool inPerformCPUDecoding, bool inIsFromPLY) : bPerformCPUDecoding(inPerformCPUDecoding), bIsFromPLY(inIsFromPLY)
    {

    }

    bool bPerformCPUDecoding;
    bool bIsFromPLY;
};

class EVERCOASTPLAYBACK_API EvercoastGaussianSplatDecoder : public IGenericDecoder
{
public:
	static std::shared_ptr< EvercoastGaussianSplatDecoder> Create();
    EvercoastGaussianSplatDecoder();
	virtual ~EvercoastGaussianSplatDecoder();

	virtual DecoderType GetType() const override;
	virtual bool DecodeMemoryStream(const uint8_t* stream, size_t stream_size, double timestamp, int64_t frameIndex, GenericDecodeOption* option) override;
	virtual std::shared_ptr<GenericDecodeResult> TakeResult() override;

private:
    bool DecodeMemoryStreamPLY(const uint8_t* stream, size_t stream_size, double timestamp, int64_t frameIndex, GenericDecodeOption* option);
    bool DecodeMemoryStreamECSPZ(const uint8_t* stream, size_t stream_size, double timestamp, int64_t frameIndex, GenericDecodeOption* option);
    std::shared_ptr<GenericDecodeResult> m_result;
};

