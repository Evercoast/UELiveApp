#include "Gaussian/EvercoastGaussianSplatCSResult.h"


EvercoastGaussianSplatCSResult::EvercoastGaussianSplatCSResult(
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

EvercoastGaussianSplatCSResult::~EvercoastGaussianSplatCSResult()
{
	InvalidateResult();
}

void EvercoastGaussianSplatCSResult::InvalidateResult()
{
	GenericDecodeResult::InvalidateResult();

	delete[] memBlock;
	memBlock = nullptr;
}

EvercoastGaussianSplatCSResult::EvercoastGaussianSplatCSResult(const EvercoastGaussianSplatCSResult& rhs) :
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

EvercoastGaussianSplatCSResult& EvercoastGaussianSplatCSResult::operator=(const EvercoastGaussianSplatCSResult& rhs)
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


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EvercoastGaussianSplatDecodeResult::EvercoastGaussianSplatDecodeResult(bool success, double timestamp, int64_t index,
	uint32_t inPointCount, uint32_t inShDegree, uint32_t inTextureSize, float* inPositions, uint8_t* inColourAlphas, float* inFloatColourAlphas,
	float* inScales, float* inRotationQuats, uint32_t* inSHCoeff_R, uint32_t* inSHCoeff_G, uint32_t* inSHCoeff_B) :
	GenericDecodeResult(success, timestamp, index),
	pointCount(inPointCount),
	shDegree(inShDegree),
	textureSize(inTextureSize),
	positions(inPositions),
	colourAlphas(inColourAlphas),
	floatColourAlphas(inFloatColourAlphas),
	scales(inScales),
	rotationQuats(inRotationQuats),
	shCoeffs_R(inSHCoeff_R),
	shCoeffs_G(inSHCoeff_G),
	shCoeffs_B(inSHCoeff_B)
{

}

EvercoastGaussianSplatDecodeResult::~EvercoastGaussianSplatDecodeResult()
{
	InvalidateResult();
}

void EvercoastGaussianSplatDecodeResult::InvalidateResult()
{
	GenericDecodeResult::InvalidateResult();

	if (positions)
	{
		delete[] positions;
		positions = nullptr;
	}

	if (colourAlphas)
	{
		delete[] colourAlphas;
		colourAlphas = nullptr;
	}

	if (floatColourAlphas)
	{
		delete[] floatColourAlphas;
		floatColourAlphas = nullptr;
	}

	if (scales)
	{
		delete[] scales;
		scales = nullptr;
	}

	if (rotationQuats)
	{
		delete[] rotationQuats;
		rotationQuats = nullptr;
	}

	// Only shCoeffs_R has ownership, other 2 are adjacent memories that allocated altogether
	if (shCoeffs_R)
	{
		delete[] shCoeffs_R;
		shCoeffs_R = nullptr;
	}
}

