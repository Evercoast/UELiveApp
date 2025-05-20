#include "Gaussian/EvercoastGaussianSplatDecoder.h"
#include "EvercoastVoxelDecoder.h" // log define
#include "Gaussian/EvercoastGaussianSplatPassthroughResult.h"
#include "zstd.h"
#include <cmath>


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

	// TODO:
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



std::shared_ptr<EvercoastGaussianSplatDecoder> EvercoastGaussianSplatDecoder::Create()
{
	return std::shared_ptr<EvercoastGaussianSplatDecoder>(new EvercoastGaussianSplatDecoder());
}

EvercoastGaussianSplatDecoder::~EvercoastGaussianSplatDecoder()
{
	m_result.reset();
}

DecoderType EvercoastGaussianSplatDecoder::GetType() const
{
	return DecoderType::DT_EvercoastSpz;
}

std::shared_ptr<GenericDecodeResult> EvercoastGaussianSplatDecoder::TakeResult()
{
	return std::move(m_result);
}

struct TransformResult
{
	uint32_t pointCount{ 0 };
	uint32_t shDegree{ 0 };
	float positionScalar{ 0.0f };
	uint32_t textureSize{ 0 };
	uint32_t frameNumber{ 0 };
};


// Convert on CPU first
static float extractScale(uint8_t value) {

	// Turns out scale has to be calculated as exp( float(value) / 16.0f - 10.0f)
	// The spz format has a * 2.0f term at the end though. There must be either eigenvalue extraction or covariance Sigma construction error
	// lead to "too flat" gaussian falloffs
	return expf(float(value) / 16.0f - 10.0f);// *2.0f;
}

// Convert on CPU first
// Before it gets lost Joel found an issue in the ecspz decode. For decoding the rotation x, y, and z uint8 values, they should be converted with float xf = float(xu8) / 127.5 - 1.0, replacing 127.0 with 127.5
static float extractRotation(uint8_t value) {
	return float(value) / 127.5 - 1.0;
}

static float invSigmoid(float x) { return std::log(x / (1.0f - x)); }

static uint32_t nextRoundPow2(uint32_t v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;

	return v;
}

bool EvercoastGaussianSplatDecoder::DecodeMemoryStream(const uint8_t* stream, size_t stream_size, double timestamp, int64_t frameIndex, GenericDecodeOption* option)
{
	// zstd decompression

	auto decompressedSize = ZSTD_getFrameContentSize(stream, stream_size);
	if (decompressedSize == ZSTD_CONTENTSIZE_ERROR || decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN)
	{
		UE_LOG(EvercoastVoxelDecoderLog, Error, TEXT("Getting frame compressed metadata error. Data: %P, Size: %d"), stream, stream_size);

		return false;
	}
	else
	{
		// TOOD: protect by smart ptr
		uint8_t* rawBytes = new uint8_t[decompressedSize];
		auto actualDecompressedSize = ZSTD_decompress(rawBytes, decompressedSize, stream, stream_size);

		// rawBytes should contain an ECSPZ frame
		ECSpzHeader* header = (ECSpzHeader*)rawBytes;
//		UE_LOG(EvercoastDecoderLog, Log, TEXT("SPZ header magic: 0x%08x version: %d pointCount: %d frameNumber: %d"), header->magic, header->version, header->pointCount, header->frameNumber);

		// Wrong version or wrong content
		if (header->magic != 0x50534345 || header->version != 1)
		{
			UE_LOG(EvercoastVoxelDecoderLog, Error, TEXT("Wrong SPZ header magic: 0x%08x or version: %d"), header->magic, header->version);

			delete[] rawBytes;
			return false;
		}

		uint32_t pointCount = header->pointCount;
		uint32_t shDegree = header->shDegree;
		// SH dimension will be 
			 // 0 when SHDegree = 0, 
			 // 3 when SHDegree = 1, 
			 // 8 when SHDegree = 2,
			 // 15 when SHDegree = 3
		uint32_t SHDim = (shDegree + 1) * (shDegree + 1) - 1;
		float positionScalar = 1.0f / static_cast<float>(1 << header->fractionalBits);

		EvercoastGaussianSplatDecodeOption* decodingOption = (EvercoastGaussianSplatDecodeOption*)option;
		if (decodingOption && decodingOption->bPerformCPUDecoding)
		{

			// TODO: correctly work out texture dimension, considering the width limit
			//uint32_t textureSize = sqrt(pointCount) + 1;
			uint32_t textureSize = sqrt(pointCount);
			textureSize = nextRoundPow2(textureSize);

			TransformResult result{
				pointCount,
				shDegree,
				positionScalar,
				textureSize,
				header->frameNumber
			};

			const uint8_t* packedPositions = rawBytes + sizeof(ECSpzHeader);
			const uint8_t* packedAlphas = packedPositions + 3 * 3 * pointCount; // offset prev 24 bit fixed point signed integer, x,y,z (mean)
			const uint8_t* packedAlphas2 = packedPositions + 3 * 3 * pointCount; // offset prev 24 bit fixed point signed integer, x,y,z (mean)

			const uint8_t* packedColours = packedAlphas + 1 * pointCount; // offset prev 8 bit unsigned integer (opacity)
			const uint8_t* packedColours2 = packedAlphas + 1 * pointCount; // offset prev 8 bit unsigned integer (opacity)

			const uint8_t* packedScales = packedColours + 3 * pointCount; // offset prev 8 bit unsigned integer, r,g,b (diffuse color)
			const uint8_t* packedRotations = packedScales + 3 * pointCount; // offset prev 8 bit log encoded integer, sx, sy, sz(scale)
			const uint8_t* packedSHCoeffs = packedRotations + 3 * pointCount; // offset prev 8 bit signed integer rx, ry, rz(rotation quaternion), w will be calculated on-the-fly

			// unpack positions

			float* DecodedPos = new float[4 * pointCount]; // padding 3+1 for A32F B32F G32F R32F
			float* outPos = DecodedPos;

			for (uint32_t i = 0; i < pointCount; ++i)
			{
				for (uint32_t j = 0; j < 3; ++j)
				{
					int32_t v = *packedPositions++;
					v |= *packedPositions++ << 8;
					v |= *packedPositions++ << 16;
					v |= v & 0x800000 ? static_cast<int32_t>(0xff000000) : 0;
					*DecodedPos++ = static_cast<float>(v) * result.positionScalar;
				}

				*DecodedPos++ = 0.0f;
			}

			// unpack colour+alpha
			uint8_t* DecodedColourAlpha = new uint8_t[4 * pointCount];
			uint8_t* outColorAlpha = DecodedColourAlpha;

			for (uint32_t i = 0; i < pointCount; ++i)
			{
				// RGB
				for (uint32_t j = 0; j < 3; ++j)
				{
					*DecodedColourAlpha++ = *packedColours++;
				}
				// A
				*DecodedColourAlpha++ = *packedAlphas++;
			}

			float* DecodedFloatColourAlpha = new float[4 * pointCount];
			float* outFloatColorAlpha = DecodedFloatColourAlpha;
			for (uint32_t i = 0; i < pointCount; ++i)
			{
				// RGB
				for (uint32_t j = 0; j < 3; ++j)
				{
					uint8_t c = *packedColours2++;
					*DecodedFloatColourAlpha++ = (float)c / 255.0f;
				}
				// A
				uint8_t a = *packedAlphas2++;
				*DecodedFloatColourAlpha++ = (float)a / 255.0f;
			}

			float* DecodedScale = new float[4 * pointCount]; // padding 3+1
			float* outScale = DecodedScale;
			for (uint32_t i = 0; i < pointCount; ++i)
			{
				// sx, sy, sz
				for (uint32_t j = 0; j < 3; ++j)
				{
					*DecodedScale++ = extractScale(*packedScales++);
				}

				*DecodedScale++ = 0.0f; // padding
			}

			float* DecodedQuat = new float[4 * pointCount];
			float* outQuat = DecodedQuat;
			for (uint32_t i = 0; i < pointCount; ++i)
			{
				float* quat = DecodedQuat;
				// rx, ry, rz
				for (uint32_t j = 0; j < 3; ++j)
				{
					*DecodedQuat++ = extractRotation(*packedRotations++);
				}

				// w - calculate on CPU first
				*DecodedQuat++ = sqrt(std::max(0.0f, 1.0f - (quat[0] * quat[0] + quat[1] * quat[1] + quat[2] * quat[2])));
			}

			// unpack position + scale + rotation to:
			/**
			 * uint32[4]
			 *  u24 pos x, u8 scl x,
			 *  u24 pos y, u8 scl y,
			 *  u24 pos z, u8 scl z,
			 *  u8 rot xyz, 0
			 *
			 *  byte layout
			 *  x0, x1, x2, sx, y0, y1, y2, sy, z0, z1, z2, sz, rx, ry, rz, 0
			 */
			 /*
			 // reset
			 packedPositions = rawBytes + sizeof(ECSpzHeader);

			 uint32_t* DecodedTransformA = new uint32_t[4 * pointCount];
			 uint8_t* DecodedTransformAWriter = (uint8_t*)DecodedTransformA;
			 uint32_t* outTransformA= DecodedTransformA;
			 for (uint32_t i = 0; i < pointCount; ++i)
			 {
				 // X : 24bit pos. 8bit scale
				 *DecodedTransformAWriter++ = *packedPositions++;
				 *DecodedTransformAWriter++ = *packedPositions++;
				 *DecodedTransformAWriter++ = *packedPositions++;
				 *DecodedTransformAWriter++ = *packedScales++;

				 // Y : 24bit pos. 8bit scale
				 *DecodedTransformAWriter++ = *packedPositions++;
				 *DecodedTransformAWriter++ = *packedPositions++;
				 *DecodedTransformAWriter++ = *packedPositions++;
				 *DecodedTransformAWriter++ = *packedScales++;

				 // Z : 24bit pos. 8bit scale
				 *DecodedTransformAWriter++ = *packedPositions++;
				 *DecodedTransformAWriter++ = *packedPositions++;
				 *DecodedTransformAWriter++ = *packedPositions++;
				 *DecodedTransformAWriter++ = *packedScales++;

				 // XYZ : Rotation
				 *DecodedTransformAWriter++ = *packedRotations++;
				 *DecodedTransformAWriter++ = *packedRotations++;
				 *DecodedTransformAWriter++ = *packedRotations++;
				 *DecodedTransformAWriter++ = 0;

			 }
			 */



			 
			// Every colour channel(R,G,B)'s SH coefficient will take 16 bytes, that's 4 x uint32_t
			uint32_t* DecodedSHCoeffs = new uint32_t[4 * pointCount * 3]; // SH_R, SH_G, SH_B

			// Interleaved SH coeff to become independent 3 pointers, although the memory layout will be all the R coefficients, followed by G, followed by B
			uint8_t* DecodedSHCoeffs_R = ((uint8_t*)DecodedSHCoeffs);
			uint8_t* DecodedSHCoeffs_G = ((uint8_t*)DecodedSHCoeffs) + sizeof(uint32_t) * 4 * pointCount;
			uint8_t* DecodedSHCoeffs_B = ((uint8_t*)DecodedSHCoeffs) + sizeof(uint32_t) * 8 * pointCount;

			uint32_t* outSHCoeff_R = (uint32_t*)DecodedSHCoeffs_R;
			uint32_t* outSHCoeff_G = (uint32_t*)DecodedSHCoeffs_G;
			uint32_t* outSHCoeff_B = (uint32_t*)DecodedSHCoeffs_B;

			if (shDegree > 0)
			{
				for (uint32_t i = 0; i < pointCount; ++i)
				{
					uint32_t d = 0;
					for (; d < SHDim; ++d)
					{
						*DecodedSHCoeffs_R++ = *packedSHCoeffs++;
						*DecodedSHCoeffs_G++ = *packedSHCoeffs++;
						*DecodedSHCoeffs_B++ = *packedSHCoeffs++;
					}

					const uint32_t skip = 16 - d;

					// DEBUG: clear the skipped bytes too
#if 1
					for (uint32_t k = 0; k < skip; ++k)
					{
						*DecodedSHCoeffs_R++ = 0;
						*DecodedSHCoeffs_G++ = 0;
						*DecodedSHCoeffs_B++ = 0;
					}
#else
					DecodedSHCoeffs_R += skip; // empty coeff missing dimension
					DecodedSHCoeffs_G += skip;
					DecodedSHCoeffs_B += skip;
#endif
				}
			}

			uint32_t* outSHCoeffAll = DecodedSHCoeffs;

			m_result = std::make_shared<EvercoastGaussianSplatDecodeResult>(true, timestamp, frameIndex, pointCount, shDegree, textureSize, outPos, outColorAlpha, outFloatColorAlpha, outScale, outQuat,
				outSHCoeff_R, outSHCoeff_G, outSHCoeff_B);

			delete[] rawBytes;
			return true;
		}
		else
		{

			// Pointers from original raw buffer
			const uint8_t* packedPositions = rawBytes + sizeof(ECSpzHeader);
			const uint32_t packedPositionsSize = 3 * 3 * pointCount; // 24 bit fixed point signed integer, x,y,z (mean)

			const uint8_t* packedAlphas = packedPositions + packedPositionsSize; // offset prev 24 bit fixed point signed integer, x,y,z (mean)
			const uint32_t packedAlphasSize = 1 * pointCount;
			const uint8_t* packedColours = packedAlphas + packedAlphasSize; // offset prev 8 bit unsigned integer (opacity)
			const uint32_t packedColoursSize = 3 * pointCount;
			const uint8_t* packedScales = packedColours + packedColoursSize;
			const uint32_t packedScalesSize = 3 * pointCount; // 8bit x 3
			const uint8_t* packedRotations = packedScales + packedScalesSize;
			const uint32_t packedRotationsSize = 3 * pointCount; // 8bit x 3

			// Create padded raw buffer
			uint32_t paddedRawBufferSize = sizeof(ECSpzHeader) +
				3 * 4 * pointCount + // position 
				4 * pointCount +	 // diffuse(SH0) + opacity
				4 * pointCount +	 // scale
				4 * pointCount;  	 // rotation

			uint32_t paddedSHCoeffsSize = 0;
			if (SHDim == 1)
			{
				paddedSHCoeffsSize = 4 * 3 * pointCount;
			}
			else if (SHDim == 2)
			{
				paddedSHCoeffsSize = 8 * 3 * pointCount;
			}
			else if (SHDim == 3)
			{
				paddedSHCoeffsSize = 16 * 3 * pointCount;
			}

			paddedRawBufferSize += paddedSHCoeffsSize;

			uint8_t* paddedRawBuffer = new uint8_t[paddedRawBufferSize];
			memcpy(paddedRawBuffer, rawBytes, sizeof(ECSpzHeader));  // copy header


			uint32_t packed4BytesAlignedPositionSize = 3 * 4 * pointCount;
			uint8_t* packed4BytesAlignedPositions = paddedRawBuffer + sizeof(ECSpzHeader);

			// Copy position with padding of 4 bytes
			uint32_t writeIdx = 0;
			for (uint32_t i = 0; i < packedPositionsSize; ++i)
			{
				packed4BytesAlignedPositions[writeIdx++] = packedPositions[i];

				if (i % 3 == 2)
				{
					packed4BytesAlignedPositions[writeIdx++] = 0;
				}
			}

			
			
			//uint8_t* packed4BytesAlignedAlphas = packed4BytesAlignedPositions + packed4BytesAlignedPositionSize;
			//uint32_t packed4BytesAlignedAlphasSize = 1 * 4 * pointCount; // 8 bit unsigned integer (opacity)

			uint8_t* packed4BytesAlignedColourAlphas = packed4BytesAlignedPositions + packed4BytesAlignedPositionSize; // offset prev 
			uint32_t packed4BytesAlignedColourAlphasSize = 4 * pointCount; // 8 bit unsigned integer, r,g,b (diffuse color) and a(opacity)

			// Copy RGB + alpha (r,g,b,a)
			writeIdx = 0;
			for (uint32_t i = 0; i < pointCount; ++i)
			{
				packed4BytesAlignedColourAlphas[writeIdx++] = packedColours[i * 3 + 0]; // R
				packed4BytesAlignedColourAlphas[writeIdx++] = packedColours[i * 3 + 1]; // G
				packed4BytesAlignedColourAlphas[writeIdx++] = packedColours[i * 3 + 2]; // B
				packed4BytesAlignedColourAlphas[writeIdx++] = packedAlphas[i];			// A
			}

			// TODO: padding copy scale, rotation, and SH

			uint8_t* packed4BytesAlignedScales = packed4BytesAlignedColourAlphas + packed4BytesAlignedColourAlphasSize;
			uint32_t packed4BytesAlignedScalesSize = 4 * pointCount; //  8 bit log encoded integer, sx, sy, sz(scale)

			// Copy scale, (sx,sy,sz,padding)
			writeIdx = 0;
			for (uint32_t i = 0; i < pointCount; ++i)
			{
				packed4BytesAlignedScales[writeIdx++] = packedScales[i * 3 + 0];
				packed4BytesAlignedScales[writeIdx++] = packedScales[i * 3 + 1];
				packed4BytesAlignedScales[writeIdx++] = packedScales[i * 3 + 2];
				packed4BytesAlignedScales[writeIdx++] = 0;
			}

			// Copy rotation (rx, ry, rz, rw)
			uint8_t* packed4BytesAlignedRotations = packed4BytesAlignedScales + packed4BytesAlignedScalesSize;
			uint32_t packed4BytesAlignedRotationsSize = 4 * pointCount; // 8 bit signed integer rx, ry, rz(rotation quaternion), w will be calculated on-the-fly
			writeIdx = 0;
			for (uint32_t i = 0; i < pointCount; ++i)
			{
				packed4BytesAlignedRotations[writeIdx++] = packedRotations[i * 3 + 0];
				packed4BytesAlignedRotations[writeIdx++] = packedRotations[i * 3 + 1];
				packed4BytesAlignedRotations[writeIdx++] = packedRotations[i * 3 + 2];
				packed4BytesAlignedRotations[writeIdx++] = 0; // w - will be calc on GPU
			}

			uint8_t* packed4BytesAlignedSHCoeffs = packed4BytesAlignedRotations + packed4BytesAlignedRotationsSize;
			uint32_t packed4BytesAlignedSHCoeffsSize = paddedSHCoeffsSize; // 8 bit for each R, G, B channel per coefficient. Coefficient count determined by SHDim

			m_result = std::make_shared <EvercoastGaussianSplatPassthroughResult>(true, timestamp, frameIndex, pointCount, shDegree, positionScalar, paddedRawBuffer, paddedRawBufferSize,
				packed4BytesAlignedPositions, packed4BytesAlignedPositionSize,
				packed4BytesAlignedColourAlphas, packed4BytesAlignedColourAlphasSize,
				packed4BytesAlignedScales, packed4BytesAlignedScalesSize,
				packed4BytesAlignedRotations, packed4BytesAlignedRotationsSize,
				packed4BytesAlignedSHCoeffs, packed4BytesAlignedSHCoeffsSize);
			
			delete[] rawBytes;

			return true;
		}
	}

}