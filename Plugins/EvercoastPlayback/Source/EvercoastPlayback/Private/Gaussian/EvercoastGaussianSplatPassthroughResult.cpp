#include "Gaussian/EvercoastGaussianSplatPassthroughResult.h"


EvercoastGaussianSplatPassthroughResult::EvercoastGaussianSplatPassthroughResult(
	bool success, double timestamp, int64_t index,
	uint32_t inPointCount, uint32_t inShDegree, float inPositionScalar, uint8_t* wholeMemoryBlock, uint32_t wholeMemoryBlockSize,
	uint8_t* inPositions, uint32_t inPositionsSize,
	uint8_t* inColourAlphas, uint32_t inColourAlphasSize,
	uint8_t* inScales, uint32_t inScalesSize,
	uint8_t* inRotations, uint32_t inRotationsSize,
	uint8_t* inSHCoeffs, uint32_t inSHCoeffsSize) :
	GenericDecodeResult(success, timestamp, index),
	pointCount(inPointCount),
	shDegree(inShDegree),
	positionScalar(inPositionScalar),
	memBlock(wholeMemoryBlock),
	memBlockSize(wholeMemoryBlockSize),
	packedPositions(inPositions), packedPositionsSize(inPositionsSize),
	packedColourAlphas(inColourAlphas), packedColourAlphasSize(inColourAlphasSize),
	packedScales(inScales), packedScalesSize(inScalesSize),
	packedRotations(inRotations), packedRotationsSize(inRotationsSize),
	packedSHCoeffs(inSHCoeffs), packedSHCoeffsSize(inSHCoeffsSize)
{
}

EvercoastGaussianSplatPassthroughResult::~EvercoastGaussianSplatPassthroughResult()
{
	InvalidateResult();
}

void EvercoastGaussianSplatPassthroughResult::InvalidateResult()
{
	GenericDecodeResult::InvalidateResult();

	delete[] memBlock;
	memBlock = nullptr;
}

EvercoastGaussianSplatPassthroughResult::EvercoastGaussianSplatPassthroughResult(const EvercoastGaussianSplatPassthroughResult& rhs) :
	GenericDecodeResult(rhs),
	pointCount(rhs.pointCount),
	shDegree(rhs.shDegree),
	positionScalar(rhs.positionScalar),
	memBlock(nullptr), memBlockSize(rhs.memBlockSize),
	packedPositions(nullptr), packedPositionsSize(rhs.packedPositionsSize),
	packedColourAlphas(nullptr), packedColourAlphasSize(rhs.packedColourAlphasSize),
	packedScales(nullptr), packedScalesSize(rhs.packedScalesSize),
	packedRotations(nullptr), packedRotationsSize(rhs.packedRotationsSize),
	packedSHCoeffs(nullptr), packedSHCoeffsSize(rhs.packedSHCoeffsSize)
{
	memBlock = new uint8_t[memBlockSize];
	memcpy(memBlock, rhs.memBlock, memBlockSize);
	packedPositions = memBlock + (rhs.packedPositions - rhs.memBlock);
	packedColourAlphas = memBlock + (rhs.packedColourAlphas - rhs.memBlock);
	packedScales = memBlock + (rhs.packedScales - rhs.memBlock);
	packedRotations = memBlock + (rhs.packedRotations - rhs.memBlock);
	packedSHCoeffs = memBlock + (rhs.packedSHCoeffs - rhs.memBlock);
}

EvercoastGaussianSplatPassthroughResult& EvercoastGaussianSplatPassthroughResult::operator=(const EvercoastGaussianSplatPassthroughResult& rhs)
{
	InvalidateResult();

	pointCount = rhs.pointCount;
	shDegree = rhs.shDegree;
	positionScalar = rhs.positionScalar;
	memBlockSize = rhs.memBlockSize;
	packedPositionsSize = rhs.packedPositionsSize;
	packedColourAlphasSize = rhs.packedColourAlphasSize;
	packedScalesSize = rhs.packedScalesSize;
	packedRotationsSize = rhs.packedRotationsSize;
	packedSHCoeffsSize = rhs.packedSHCoeffsSize;

	memBlock = new uint8_t[memBlockSize];
	memcpy(memBlock, rhs.memBlock, memBlockSize);
	packedPositions = memBlock + (rhs.packedPositions - rhs.memBlock);
	packedColourAlphas = memBlock + (rhs.packedColourAlphas - rhs.memBlock);
	packedScales = memBlock + (rhs.packedScales - rhs.memBlock);
	packedRotations = memBlock + (rhs.packedRotations - rhs.memBlock);
	packedSHCoeffs = memBlock + (rhs.packedSHCoeffs - rhs.memBlock);

	return *this;
}
