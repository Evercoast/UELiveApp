#include "Gaussian/EvercoastGaussianSplatDecoder.h"
#include "EvercoastVoxelDecoder.h" // log define
#include "Gaussian/EvercoastGaussianSplatCSResult.h"
#include "zstd.h"
#include <cmath>
#include "load-spz.h"


std::shared_ptr<EvercoastGaussianSplatDecoder> EvercoastGaussianSplatDecoder::Create()
{
	return std::shared_ptr<EvercoastGaussianSplatDecoder>(new EvercoastGaussianSplatDecoder());
}

EvercoastGaussianSplatDecoder::EvercoastGaussianSplatDecoder()
{
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

static bool parseSpzHeader(spz::PackedGaussiansHeader* header, size_t* out_header_size, uint32_t* out_pointCount, uint32_t* out_SHDegree, float* out_positionScalar)
{
	// Wrong version or wrong content
	if (header->magic != 0x5053474e || header->version != 2)
	{
		UE_LOG(EvercoastVoxelDecoderLog, Error, TEXT("Wrong SPZ header magic: 0x%08x or version: %d"), header->magic, header->version);
		return false;
	}

	*out_header_size = sizeof(spz::PackedGaussiansHeader);
	*out_pointCount = header->numPoints;
	*out_SHDegree = header->shDegree;
	*out_positionScalar = 1.0f / static_cast<float>(1 << header->fractionalBits);

	return true;
}

static bool parseECSpzHeader(ECSpzHeader* header, size_t* out_header_size, uint32_t* out_pointCount, uint32_t* out_SHDegree, float* out_positionScalar)
{
	// Wrong version or wrong content
	if (header->magic != 0x50534345 || header->version != 1)
	{
		UE_LOG(EvercoastVoxelDecoderLog, Error, TEXT("Wrong SPZ header magic: 0x%08x or version: %d"), header->magic, header->version);
		return false;
	}

	*out_header_size = sizeof(ECSpzHeader);
	*out_pointCount = header->pointCount;
	*out_SHDegree = header->shDegree;
	*out_positionScalar = 1.0f / static_cast<float>(1 << header->fractionalBits);
	return true;
}

static std::shared_ptr<EvercoastGaussianSplatCSResult> fillAndPadDataFromRawSPZ(uint8_t* rawBytes, double timestamp, int64_t frameIndex, 
	size_t headerOffset, uint32_t pointCount, uint32_t shDegree, float positionScalar)
{
	// Pointers from original raw buffer
	const uint8_t* packedPositions = rawBytes + headerOffset;
	const uint32_t packedPositionsSize = 3 * 3 * pointCount; // 24 bit fixed point signed integer, x,y,z (mean)

	const uint8_t* packedAlphas = packedPositions + packedPositionsSize; // offset prev 24 bit fixed point signed integer, x,y,z (mean)
	const uint32_t packedAlphasSize = 1 * pointCount;
	const uint8_t* packedColours = packedAlphas + packedAlphasSize; // offset prev 8 bit unsigned integer (opacity)
	const uint32_t packedColoursSize = 3 * pointCount;
	const uint8_t* packedScales = packedColours + packedColoursSize;
	const uint32_t packedScalesSize = 3 * pointCount; // 8bit x 3
	const uint8_t* packedRotations = packedScales + packedScalesSize;
	const uint32_t packedRotationsSize = 3 * pointCount; // 8bit x 3

	// The coefficients for a gaussian are organized such that the color channel is the inner (faster varying) axis, and the coefficient is the outer (slower varying) axis
	const uint8_t* packedSHCoeffs = packedRotations + packedRotationsSize;
	const uint32_t SHDim = (shDegree + 1) * (shDegree + 1) - 1; // shDegree <- [0, 1, 2, 3], SHDim <- [0, 3, 8, 15]
	const uint32_t packedSHCoeffsSize = SHDim * 3 * pointCount; // Each coefficient is represented as an 8-bit signed integer. So 3 bytes for RGB channels
	

	// Create padded raw buffer
	uint32_t paddedRawBufferSize = headerOffset +
		3 * 4 * pointCount + // position 
		4 * pointCount +	 // diffuse(SH0) + opacity
		4 * pointCount +	 // scale
		4 * pointCount;  	 // rotation

	// With SH
	const uint32_t paddedSHCoeffsSize = SHDim * 4 * pointCount;

	paddedRawBufferSize += paddedSHCoeffsSize;

	uint8_t* paddedRawBuffer = new uint8_t[paddedRawBufferSize];
	memcpy(paddedRawBuffer, rawBytes, headerOffset);  // copy header


	uint32_t padded4BytesAlignedPositionSize = 3 * 4 * pointCount;
	uint8_t* padded4BytesAlignedPositions = paddedRawBuffer + headerOffset;

	// Copy position with padding of 4 bytes
	uint32_t writeIdx = 0;
	for (uint32_t i = 0; i < packedPositionsSize; ++i)
	{
		padded4BytesAlignedPositions[writeIdx++] = packedPositions[i];

		// Since it's writeIdx++, writeIdx would be 1 head of i, so when i%3==2, writeIdx is multiply of 4 
		if (i % 3 == 2)
		{
			padded4BytesAlignedPositions[writeIdx++] = 0;
		}
	}

	uint8_t* padded4BytesAlignedColourAlphas = padded4BytesAlignedPositions + padded4BytesAlignedPositionSize; // offset prev 
	uint32_t padded4BytesAlignedColourAlphasSize = 4 * pointCount; // 8 bit unsigned integer, r,g,b (diffuse color) and a(opacity)

	// Copy RGB + alpha (r,g,b,a)
	writeIdx = 0;
	for (uint32_t i = 0; i < pointCount; ++i)
	{
		padded4BytesAlignedColourAlphas[writeIdx++] = packedColours[i * 3 + 0]; // R
		padded4BytesAlignedColourAlphas[writeIdx++] = packedColours[i * 3 + 1]; // G
		padded4BytesAlignedColourAlphas[writeIdx++] = packedColours[i * 3 + 2]; // B
		padded4BytesAlignedColourAlphas[writeIdx++] = packedAlphas[i];			// A
	}

	// Padding copy scale, rotation, and SH

	uint8_t* padded4BytesAlignedScales = padded4BytesAlignedColourAlphas + padded4BytesAlignedColourAlphasSize;
	uint32_t padded4BytesAlignedScalesSize = 4 * pointCount; //  8 bit log encoded integer, sx, sy, sz(scale)

	// Copy scale, (sx,sy,sz,padding)
	writeIdx = 0;
	for (uint32_t i = 0; i < pointCount; ++i)
	{
		padded4BytesAlignedScales[writeIdx++] = packedScales[i * 3 + 0];
		padded4BytesAlignedScales[writeIdx++] = packedScales[i * 3 + 1];
		padded4BytesAlignedScales[writeIdx++] = packedScales[i * 3 + 2];
		padded4BytesAlignedScales[writeIdx++] = 0;
	}

	// Copy rotation (rx, ry, rz, rw)
	uint8_t* padded4BytesAlignedRotations = padded4BytesAlignedScales + padded4BytesAlignedScalesSize;
	uint32_t padded4BytesAlignedRotationsSize = 4 * pointCount; // 8 bit signed integer rx, ry, rz(rotation quaternion), w will be calculated on-the-fly
	writeIdx = 0;
	for (uint32_t i = 0; i < pointCount; ++i)
	{
		padded4BytesAlignedRotations[writeIdx++] = packedRotations[i * 3 + 0];
		padded4BytesAlignedRotations[writeIdx++] = packedRotations[i * 3 + 1];
		padded4BytesAlignedRotations[writeIdx++] = packedRotations[i * 3 + 2];
		padded4BytesAlignedRotations[writeIdx++] = 0; // w - will be calc on GPU
	}

	// Copy SH coeffs
	uint8_t* padded4BytesAlignedSHCoeffs = padded4BytesAlignedRotations + padded4BytesAlignedRotationsSize;
	uint32_t padded4BytesAlignedSHCoeffsSize = paddedSHCoeffsSize; // 8 bit for each R, G, B channel per coefficient. An extra 8 bit for padding. Coefficient count determined by SHDim

	// Order: [N, S, C], N=Splat Index, S=SH Coeffs, C=Colour
	writeIdx = 0;
	for (uint32_t i = 0; i < pointCount; ++i)
	{
		uint32_t packedIndex = (i * SHDim * 3);
		uint32_t paddedIndex = (i * SHDim * 4);

		uint32_t d = 0;
		for (d = 0; d < SHDim; ++d)
		{
			// RGB (R in the lowest bit in little endian)
			padded4BytesAlignedSHCoeffs[paddedIndex + d * 4 + 0] = packedSHCoeffs[packedIndex + d * 3 + 0];
			padded4BytesAlignedSHCoeffs[paddedIndex + d * 4 + 1] = packedSHCoeffs[packedIndex + d * 3 + 1];
			padded4BytesAlignedSHCoeffs[paddedIndex + d * 4 + 2] = packedSHCoeffs[packedIndex + d * 3 + 2];
			// Padding
			padded4BytesAlignedSHCoeffs[paddedIndex + d * 4 + 3] = 0;
		}

	}

	return std::make_shared <EvercoastGaussianSplatCSResult>(true, timestamp, frameIndex, pointCount, shDegree, positionScalar, paddedRawBuffer, paddedRawBufferSize,
		padded4BytesAlignedPositions, padded4BytesAlignedPositionSize,
		padded4BytesAlignedColourAlphas, padded4BytesAlignedColourAlphasSize,
		padded4BytesAlignedScales, padded4BytesAlignedScalesSize,
		padded4BytesAlignedRotations, padded4BytesAlignedRotationsSize,
		padded4BytesAlignedSHCoeffs, padded4BytesAlignedSHCoeffsSize);
}

bool EvercoastGaussianSplatDecoder::DecodeMemoryStreamPLY(const uint8_t* stream, size_t stream_size, double timestamp, int64_t frameIndex, GenericDecodeOption* option)
{
	spz::UnpackOptions unpackOption;
	spz::PackOptions packOption;
	
	uint8_t* pOutSpz;
	size_t outSpzSize;
	// FIXME: it's slow but works: convert PLY to SPZ using libspz and load it with the same routine
	bool outputSpzFormat = spz::convertPlyDataToSpzData(stream, stream_size, unpackOption, &pOutSpz, &outSpzSize, packOption);
	if (!outputSpzFormat)
		return false;

	// protected by smart ptr. Note pOutSpz is allocated by malloc()!
	std::shared_ptr<uint8_t> protectedMemoryBlock(pOutSpz, [](void* p) { free(p); });

	uint8_t* rawBytes = (uint8_t*)pOutSpz;

	size_t headerOffset;
	uint32_t pointCount, shDegree;
	float positionScalar;
	spz::PackedGaussiansHeader* header = (spz::PackedGaussiansHeader*)rawBytes;

	if (!parseSpzHeader(header, &headerOffset, &pointCount, &shDegree, &positionScalar))
	{
		return false;
	}

	uint32_t SHDim = (shDegree + 1) * (shDegree + 1) - 1;
	m_result = fillAndPadDataFromRawSPZ(rawBytes, timestamp, frameIndex, headerOffset, pointCount, shDegree, positionScalar);

	return true;
}

bool EvercoastGaussianSplatDecoder::DecodeMemoryStreamECSPZ(const uint8_t* stream, size_t stream_size, double timestamp, int64_t frameIndex, GenericDecodeOption* option)
{
	size_t headerOffset;
	uint32_t pointCount, shDegree;
	float positionScalar;
	

	auto decompressedSize = ZSTD_getFrameContentSize(stream, stream_size);
	if (decompressedSize == ZSTD_CONTENTSIZE_ERROR || decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN)
	{
		UE_LOG(EvercoastVoxelDecoderLog, Error, TEXT("Getting frame compressed metadata error. Data: %p, Size: %d"), stream, stream_size);

		return false;
	}

	// protect by smart ptr
	std::shared_ptr<uint8_t> decompressedMemoryBlock(new uint8_t[decompressedSize], std::default_delete<uint8[]>());
	uint8_t* rawBytes = decompressedMemoryBlock.get();
	auto actualDecompressedSize = ZSTD_decompress(rawBytes, decompressedSize, stream, stream_size);

	ECSpzHeader* header = (ECSpzHeader*)rawBytes;
	if (!parseECSpzHeader(header, &headerOffset, &pointCount, &shDegree, &positionScalar))
	{
		return false;
	}

	uint32_t SHDim = (shDegree + 1) * (shDegree + 1) - 1;
	m_result = fillAndPadDataFromRawSPZ(rawBytes, timestamp, frameIndex, headerOffset, pointCount, shDegree, positionScalar);

	return true;
}

bool EvercoastGaussianSplatDecoder::DecodeMemoryStream(const uint8_t* stream, size_t stream_size, double timestamp, int64_t frameIndex, GenericDecodeOption* option)
{

	EvercoastGaussianSplatDecodeOption* decodingOption = (EvercoastGaussianSplatDecodeOption*)option;
	if (decodingOption && decodingOption->bIsFromPLY)
	{
		return DecodeMemoryStreamPLY(stream, stream_size, timestamp, frameIndex, option);
	}
	else
	{
		return DecodeMemoryStreamECSPZ(stream, stream_size, timestamp, frameIndex, option);
	}
}