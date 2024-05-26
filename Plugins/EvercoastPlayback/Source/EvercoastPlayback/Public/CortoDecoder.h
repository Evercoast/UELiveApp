#pragma once

#include <memory>
#include <vector>
#include <mutex>
#include "CoreMinimal.h"
#include "UnrealEngineCompatibility.h"
#include "GenericDecoder.h"

class CortoDecodeOption : public GenericDecodeOption
{
public:
	CortoDecodeOption() : bFlipTriangleWinding(false)
	{}

	CortoDecodeOption(bool flipTriangleWinding) : bFlipTriangleWinding(flipTriangleWinding)
	{}

	bool bFlipTriangleWinding;
};

class EVERCOASTPLAYBACK_API CortoDecodeResult : public GenericDecodeResult
{
public:
	CortoDecodeResult(uint32_t initVertexReserved, uint32_t initTriangleReserved);
	CortoDecodeResult(const CortoDecodeResult& rhs);
	virtual ~CortoDecodeResult();

	// for thread safety
	void Lock() const; 
	// for thread safety
	void Unlock() const;
	// Applying result is doing memcpy for now
	void ApplyResult(bool uccess, double timestamp, int64_t frameIndex, uint32_t vnum, uint32_t fnum, uint32_t* ib, FVector3f* pb, FVector2f* uvb, FVector3f* nb);
	// Make it invalidate, still keeping the buffers tho
	virtual void InvalidateResult() override;
	// Normals are optional
	bool HasNormals() const;
	// Enclarge buffers if required
	void EnsureBuffers(uint32_t vertexReserved, uint32_t traingleReserved);

	virtual DecodeResultType GetType() const override
	{
		return DecodeResultType::DRT_CortoMesh;
	}

	uint32_t VertexCount;
	uint32_t TriangleCount;

	uint32_t VertexReserved;
	uint32_t TriangleReserved;

	std::vector<uint32_t> IndexBuffer;
	std::vector<FVector3f> PositionBuffer;
	std::vector<FVector2f> UVBuffer;
	std::vector<FVector3f> NormalBuffer;

	mutable std::mutex RWLock;
};

class EVERCOASTPLAYBACK_API CortoDecoder : public IGenericDecoder
{
public:
	static constexpr int DEFAULT_VERTEX_COUNT = 100000;
	static constexpr int DEFAULT_TRIANGLE_COUNT = DEFAULT_VERTEX_COUNT;

	static std::shared_ptr<CortoDecoder> Create();
	virtual ~CortoDecoder();

	virtual DecoderType GetType() const override;
	virtual bool DecodeMemoryStream(const uint8_t* stream, size_t stream_size, double timestamp, int64_t frameIndex, GenericDecodeOption* option) override;
	virtual std::shared_ptr<GenericDecodeResult> GetResult() override;

	void SetReceivingResult(std::shared_ptr<CortoDecodeResult> receivingResult)
	{
		result = receivingResult;
	}
private:
	CortoDecoder();
	void EnsureBuffers(int vertexCount, int traingleCount);

	std::vector<uint32_t> index_buf;
	std::vector<FVector3f> position_buf;
	std::vector<FVector2f> uv_buf;
	std::vector<FVector3f> normal_buf;
	std::shared_ptr<CortoDecodeResult> result;
	int vertex_count;
	int triangle_count;
};