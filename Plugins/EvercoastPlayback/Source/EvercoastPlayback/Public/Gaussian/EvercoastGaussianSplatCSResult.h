#pragma once

#include <cstdint>
#include <memory>
#include "CoreMinimal.h"
#include "UnrealEngineCompatibility.h"
#include "GenericDecoder.h"
#include "Gaussian/GaussianSplatComputeShaderConstants.h"

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

struct EncodedSplat3DegreeSHCoeffs
{
    uint8_t coeffs[MAXIMUM_SH_COEFFS * 4]; // 4 instead of 3, which includes padding
};


// NOTE: Change of structure will need to be done in GaussianSplatTypes.ush too
struct alignas(16) SplatView
{
    float pos[4];               // local space
    float axis1[2];             // axis1 in clip space
    float axis2[2];             // axis2 in clip space
    float diffuse_opacity[4];
    float conic[4];
    int   mean_xy[2];           // mean, screen space
    float radii_clipz[2];       // my_radius, depth in clip space (repurposed to clip_pos.zw in offscreen quad renderer)
    unsigned int depth16;       // view space depth in 16bit float format stored in lower bits
    float clip_pos[3];          // position in clip space before perspective division
    int   tile_idx_ranges[4];   // covered tile index from xy(top left) to zw(bottom right)
};
#pragma pack(pop)


// Barely a result. This class does minimum data massage before sending to compute shader for recon
class EVERCOASTPLAYBACK_API EvercoastGaussianSplatCSResult : public GenericDecodeResult
{
public:
    EvercoastGaussianSplatCSResult(bool success, double timestamp, int64_t index,
        uint32_t pointCount, uint32_t shDegree, float positionScalar, uint8_t* wholeMemoryBlock, uint32_t wholeMemoryBlockSize,
        uint8_t* inPosition, uint32_t inPositionSize,
        uint8_t* inColourAlpha, uint32_t inColourAlphaSize,
        uint8_t* inScale, uint32_t inScaleSize,
        uint8_t* inRotation, uint32_t inRotationSize,
        uint8_t* inSHCoeff, uint32_t inSHCoeffSize
    );

    virtual ~EvercoastGaussianSplatCSResult();

    // Copy ctor and operator=
    EvercoastGaussianSplatCSResult(const EvercoastGaussianSplatCSResult&);
    EvercoastGaussianSplatCSResult& operator=(const EvercoastGaussianSplatCSResult&);

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


// Full CPU decoding result. Only kept for referencing
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
    uint8_t* colourAlphas;
    float* floatColourAlphas;
    float* scales;
    float* rotationQuats;

    uint32_t* shCoeffs_R;
    uint32_t* shCoeffs_G;
    uint32_t* shCoeffs_B;

};