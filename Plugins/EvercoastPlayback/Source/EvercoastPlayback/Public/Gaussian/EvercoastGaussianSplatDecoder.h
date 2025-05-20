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
    EvercoastGaussianSplatDecodeOption(bool inPerformCPUDecoding) : bPerformCPUDecoding(inPerformCPUDecoding)
    {

    }

    bool bPerformCPUDecoding;
};

class EVERCOASTPLAYBACK_API EvercoastGaussianSplatDecodeResult : public GenericDecodeResult
{
public:
    EvercoastGaussianSplatDecodeResult(bool success, double timestamp, int64_t index, 
        uint32_t pointCount, uint32_t shDegree, uint32_t textureSize, float* inPositions, uint8_t* inColourAlphas, float* inFloatColourAlphas, 
        float* inScales, float* inRotationQuats, uint32_t* inSHCoeffients_R, uint32_t* inSHCoeffients_G, uint32_t* inSHCoeffients_B);
    virtual ~EvercoastGaussianSplatDecodeResult();

    virtual DecodeResultType GetType() const override
    {
        return DecodeResultType::DRT_GaussianSplat;
    }

    virtual void InvalidateResult() override;

    uint32_t pointCount;
    uint32_t shDegree;
    float positionScalar;
    uint32_t textureSize;
    uint32_t frameNumber;
    float* positions;
    //uint32_t* transformA;
    uint8_t* colourAlphas;
    float* floatColourAlphas;
    float* scales;
    float* rotationQuats;

    uint32_t* shCoeffs_R;
    uint32_t* shCoeffs_G;
    uint32_t* shCoeffs_B;

};

class EVERCOASTPLAYBACK_API EvercoastGaussianSplatDecoder : public IGenericDecoder
{
public:
	static std::shared_ptr< EvercoastGaussianSplatDecoder> Create();
	virtual ~EvercoastGaussianSplatDecoder();

	virtual DecoderType GetType() const override;
	virtual bool DecodeMemoryStream(const uint8_t* stream, size_t stream_size, double timestamp, int64_t frameIndex, GenericDecodeOption* option) override;
	virtual std::shared_ptr<GenericDecodeResult> TakeResult() override;

private:
    std::shared_ptr<GenericDecodeResult> m_result;
};

