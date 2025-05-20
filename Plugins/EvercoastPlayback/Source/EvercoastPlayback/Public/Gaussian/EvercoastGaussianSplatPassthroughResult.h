#pragma once

#include <cstdint>
#include <memory>
#include "CoreMinimal.h"
#include "UnrealEngineCompatibility.h"
#include "GenericDecoder.h"


#pragma pack(push, 1)

struct FixedPoint24b
{
    uint8_t encoded[3];
    uint8_t padding;
};
struct EncodedSplatVector3
{
    FixedPoint24b xyz[3];
};

struct EncodedSplatColourAlpha
{
    uint8_t colour[3];
    uint8_t alpha;
};

struct EncodedSplatScale
{
    uint8_t scale[3];
    uint8_t padding;
};

struct EncodedSplatRotation
{
    uint8_t quat[4];
};


struct EncodedSplat1DegreeSHCoeff
{
    uint8_t coeffs[3];
    uint8_t padding;
};

struct EncodedSplat2DegreeSHCoeff
{
    uint8_t coeffs[8];
};

struct EncodedSplat3DegreeSHCoeff
{
    uint8_t coeffs[15];
    uint8_t padding;
};


//struct alignas(16) SplatView
struct SplatView
{
    float pos[4];
    float axis1[2];
    float axis2[2];
    float diffuse_opacity[4];
    float conic[4];
    /*
    float DBG_rotation[4];
    float DBG_scale[4];
    float DBG_RS[16];
    float DBG_cov3d0[4];
    float DBG_cov3d1[4];
    float DBG_cov2d[4];
    */
    float spr_size_rot[4];
};
#pragma pack(pop)



class EVERCOASTPLAYBACK_API EvercoastGaussianSplatPassthroughResult : public GenericDecodeResult
{
public:
    EvercoastGaussianSplatPassthroughResult(bool success, double timestamp, int64_t index,
        uint32_t pointCount, uint32_t shDegree, float positionScalar, uint8_t* wholeMemoryBlock, uint32_t wholeMemoryBlockSize,
        uint8_t* inPosition, uint32_t inPositionSize,
        uint8_t* inColourAlpha, uint32_t inColourAlphaSize,
        uint8_t* inScale, uint32_t inScaleSize,
        uint8_t* inRotation, uint32_t inRotationSize,
        uint8_t* inSHCoeff, uint32_t inSHCoeffSize
    );

    virtual ~EvercoastGaussianSplatPassthroughResult();

    // Copy ctor and operator=
    EvercoastGaussianSplatPassthroughResult(const EvercoastGaussianSplatPassthroughResult&);
    EvercoastGaussianSplatPassthroughResult& operator=(const EvercoastGaussianSplatPassthroughResult&);

    virtual DecodeResultType GetType() const override
    {
        return DecodeResultType::DRT_GaussianSplat;
    }

    virtual void InvalidateResult() override;

    uint32 GetSizeInBytes() const
    {
        return memBlockSize;
    }

    // metadata
    uint32_t pointCount;
    uint32_t shDegree;
    float positionScalar;

    // pointer for memory management, only this pointer needs to be transferred and freed
    uint8_t* memBlock;
    uint32_t memBlockSize;

    // interleaved attributes data
    uint8_t* packedPositions;
    uint32_t packedPositionsSize;

    uint8_t* packedColourAlphas;
    uint32_t packedColourAlphasSize;

    uint8_t* packedScales;
    uint32_t packedScalesSize;

    uint8_t* packedRotations;
    uint32_t packedRotationsSize;

    uint8_t* packedSHCoeffs;
    uint32_t packedSHCoeffsSize;


};