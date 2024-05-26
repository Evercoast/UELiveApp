#include "CortoDecoder.h"
#include "EvercoastDecoder.h"
#include "corto_decoder_c.h"

CortoDecodeResult::CortoDecodeResult(uint32_t initVertexCount, uint32_t initTriangleCount) :
	GenericDecodeResult(false, 0, 0),
	VertexCount(0),
	TriangleCount(0),
	VertexReserved(initVertexCount),
	TriangleReserved(initTriangleCount)
{
	if (VertexReserved > 0 && TriangleReserved > 0)
	{
		IndexBuffer.resize(TriangleReserved*3);
		PositionBuffer.resize(VertexReserved);
		UVBuffer.resize(VertexReserved);
		NormalBuffer.resize(VertexReserved);
	}
	else
	{
		IndexBuffer.resize(0);
		PositionBuffer.resize(0);
		UVBuffer.resize(0);
		NormalBuffer.resize(0);
	}
}

CortoDecodeResult::CortoDecodeResult(const CortoDecodeResult& rhs) :
	GenericDecodeResult(rhs.DecodeSuccessful, rhs.frameTimestamp, rhs.frameIndex),
	VertexCount(rhs.VertexCount),
	TriangleCount(rhs.TriangleCount),
	VertexReserved(rhs.VertexReserved),
	TriangleReserved(rhs.TriangleReserved),
	IndexBuffer(rhs.IndexBuffer),
	PositionBuffer(rhs.PositionBuffer),
	UVBuffer(rhs.UVBuffer),
	NormalBuffer(rhs.NormalBuffer)
{
}

CortoDecodeResult::~CortoDecodeResult()
{
}

void CortoDecodeResult::EnsureBuffers(uint32_t newVertexCount, uint32_t newTriangleCount)
{
	if (newVertexCount > VertexReserved)
	{
		VertexReserved = newVertexCount;

		PositionBuffer.resize(VertexReserved);
		UVBuffer.resize(VertexReserved);
		NormalBuffer.resize(VertexReserved);
	}

	if (newTriangleCount > TriangleReserved)
	{
		TriangleReserved = newTriangleCount;
		IndexBuffer.resize(TriangleReserved * 3);
	}
}

void CortoDecodeResult::ApplyResult(bool success, double timestamp, int64_t theFrameIndex, uint32_t vnum, uint32_t fnum, uint32_t* ib, FVector3f* pb, FVector2f* uvb, FVector3f* nb)
{
	Lock();

	this->DecodeSuccessful = success;
	this->frameTimestamp = timestamp;
	this->frameIndex = theFrameIndex;

	VertexCount = vnum;
	TriangleCount = fnum;

	EnsureBuffers(VertexCount, TriangleCount);

	if (vnum > 0 && fnum > 0)
	{
		memcpy(IndexBuffer.data(), ib, TriangleCount * sizeof(uint32_t) * 3);
		/*
		for (uint32_t i = 0; i < TriangleCount * 3; i += 3)
		{
			std::swap(IndexBuffer[i + 1], IndexBuffer[i + 2]);
		}
		*/

		memcpy(PositionBuffer.data(), pb, VertexCount * sizeof(FVector3f));
		memcpy(UVBuffer.data(), uvb, VertexCount * sizeof(FVector2f));
		if (nb)
		{
			memcpy(NormalBuffer.data(), nb, VertexCount * sizeof(FVector3f));
		}
		else
		{
			// we don't have normals
			NormalBuffer.resize(0);
		}
	}

	Unlock();
}

void CortoDecodeResult::InvalidateResult()
{
	Lock();

	this->DecodeSuccessful = false;
	this->frameTimestamp = 0;
	this->frameIndex = -1;

	Unlock();
}

void CortoDecodeResult::Lock() const
{
	RWLock.lock();
}

void CortoDecodeResult::Unlock() const
{
	RWLock.unlock();
}

bool CortoDecodeResult::HasNormals() const
{
	return NormalBuffer.size() > 0;
}

std::shared_ptr<CortoDecoder> CortoDecoder::Create()
{
	return std::shared_ptr<CortoDecoder>(new CortoDecoder());
}

constexpr int CortoDecoder::DEFAULT_VERTEX_COUNT;
constexpr int CortoDecoder::DEFAULT_TRIANGLE_COUNT;

CortoDecoder::CortoDecoder() :
	vertex_count(0),
	triangle_count(0)
{

	EnsureBuffers(DEFAULT_VERTEX_COUNT, DEFAULT_TRIANGLE_COUNT);
	
}

void CortoDecoder::EnsureBuffers(int vertexCount, int triangleCount)
{
	if (vertexCount > vertex_count)
	{
		position_buf.resize(vertexCount);
		uv_buf.resize(vertexCount);
		normal_buf.resize(vertexCount);

		vertex_count = vertexCount;
	}

	if (triangleCount > triangle_count)
	{
		index_buf.resize(triangleCount * 3);

		triangle_count = triangleCount;
	}
}

CortoDecoder::~CortoDecoder()
{
	result.reset();
}

DecoderType CortoDecoder::GetType() const
{
	return DT_CortoMesh;
}

bool CortoDecoder::DecodeMemoryStream(const uint8_t* stream, size_t stream_size, double timestamp, int64_t frameIndex, GenericDecodeOption* option)
{
	Corto_DecoderInfo info{ 0, 0, 0 };
	void* decoder = Corto_CreateDecoder(stream_size, (unsigned char*)stream, &info);

	// retrieve some metadata
	uint32_t triangleCount = info.nface;
	uint32_t vertexCount = info.nvert;
	bool hasNormal = info.hasNormal > 0 ? true : false;
	EnsureBuffers((int)vertexCount, (int)triangleCount);

	//UE_LOG(EvercoastDecoderLog, Log, TEXT("Frame: %d, tri: %d vert: %d"), frameIndex, triangleCount, vertexCount);
	
	Corto_DecodeMesh(decoder, (Corto_Vector3*)position_buf.data(), (uint32_t*)index_buf.data(), (Corto_Vector3*)normal_buf.data(), nullptr, (Corto_Vector2*)uv_buf.data());

	CortoDecodeOption* cortoDecodeOption = (CortoDecodeOption*)option;
	if (cortoDecodeOption->bFlipTriangleWinding)
	{
		// flip triangle index (0, 1, 2) to (0, 2, 1)
		for (uint32_t i = 0; i < index_buf.size(); i += 3)
		{
			std::swap(index_buf[i + 1], index_buf[i + 2]);
		}
	}

	result->ApplyResult(true, timestamp, frameIndex, vertexCount, triangleCount, index_buf.data(), position_buf.data(), uv_buf.data(), hasNormal ? normal_buf.data() : nullptr);
	
	Corto_DestroyDecoder(decoder);
	return true;
}

std::shared_ptr<GenericDecodeResult> CortoDecoder::GetResult()
{
	return result;
}
