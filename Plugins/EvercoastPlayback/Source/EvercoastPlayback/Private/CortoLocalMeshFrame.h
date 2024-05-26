#pragma once
#include <vector>
#include "CoreMinimal.h"
#include "CortoDecoder.h"
#include "UObject/GCObject.h"
#include "Engine/Texture.h"

struct CortoWebpUnifiedDecodeResult;
struct CortoLocalMeshFrame
{
	CortoLocalMeshFrame(const CortoWebpUnifiedDecodeResult* pResult);
	~CortoLocalMeshFrame() = default;

	uint32_t GetIndexCount() const
	{
		return m_triangleCount * 3;
	}

	uint32_t GetVertexCount() const
	{
		return m_vertexCount;
	}

	uint32_t m_vertexCount;
	uint32_t m_triangleCount;

	TArray<int32_t> m_indexBuffer; // should be uint32_t but UProceduralMeshComponent accepts only TArray<int32>
	TArray<FVector3f> m_positionBuffer;
	TArray<FVector2f> m_uvBuffer;
	TArray<FVector3f> m_normalBuffer; // optional

	FBoxSphereBounds3f m_bounds;

	FBoxSphereBounds3f GetBounds() const;
};


class CortoLocalTextureFrame : public FGCObject
{
	UTexture* m_localTexture;
	bool m_needsSwizzle;
public:
	CortoLocalTextureFrame(const CortoWebpUnifiedDecodeResult* pResult);
	virtual ~CortoLocalTextureFrame();

	UTexture* GetTexture()
	{
		return m_localTexture;
	}
    
    void UpdateTexture(const CortoWebpUnifiedDecodeResult* pResult);

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual FString GetReferencerName() const override
	{
		return TEXT("CortoLocalTextureFrame");
	}

	bool NeedsSwizzle() const
	{
		return m_needsSwizzle;
	}
};
