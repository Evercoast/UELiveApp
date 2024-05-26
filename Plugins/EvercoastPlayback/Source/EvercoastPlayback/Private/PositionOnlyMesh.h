#pragma once
#include "CoreMinimal.h"
#include "RenderResource.h"
#include "RHICommandList.h"
#include "HAL/UnrealMemory.h"
#include "UnrealEngineCompatibility.h"

#pragma pack(push, 1)
struct FPositionOnlyVertex
{
public:
	FVector3f Position;
};
#pragma pack(pop)

class FPositionOnlyVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual ~FPositionOnlyVertexDeclaration() {}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
#else
	virtual void InitRHI() override
#endif
	{
		FVertexDeclarationElementList Elements;
		uint32 Stride = sizeof(FPositionOnlyVertex);
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FPositionOnlyVertex, Position), VET_Float3, 0, Stride));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

class FPositionOnlyVertexBuffer : public FVertexBuffer
{
public:
	/** Initialize the RHI for this rendering resource */
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
	void InitRHI(FRHICommandListBase& RHICmdList) override
#else
	void InitRHI() override
#endif
	{
		if (Vertices.Num() <= 0)
			return;
		// Create vertex buffer. Fill buffer with initial data upon creation
#if ENGINE_MAJOR_VERSION == 5
		FRHIResourceCreateInfo CreateInfo(TEXT("FPositionOnlyVertexBuffer"), &Vertices);
#else
		FRHIResourceCreateInfo CreateInfo(&Vertices);
#endif
		
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
		VertexBufferRHI = RHICmdList.CreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfo);
#else
		VertexBufferRHI = RHICreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfo);
#endif
	}

	void Init(uint32 NumVertices)
	{
		Vertices.SetNumZeroed(NumVertices);
	}

	virtual void ReleaseRHI()
	{
		VertexBufferRHI.SafeRelease();
	}

	virtual FString GetFriendlyName() const override
	{
		return TEXT("FPositionOnlyVertexBuffer");
	}

	uint32 GetNumVertices() const
	{
		return (uint32)Vertices.Num();
	}

	// Vertex data accessors.
	FORCEINLINE FVector3f& VertexPosition(uint32 VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return Vertices[VertexIndex].Position;
	}
	FORCEINLINE const FVector3f& VertexPosition(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return Vertices[VertexIndex].Position;
	}

	void CopyVertices(const FPositionOnlyVertex* srcVertices, uint32 Num)
	{
		static_assert(sizeof(FPositionOnlyVertex) == sizeof(FVector3f));
		if (Num > GetNumVertices())
		{
			Vertices.SetNumZeroed(Num);
		}

		FMemory::Memcpy(Vertices.GetData(), srcVertices, Num * sizeof(FPositionOnlyVertex));
	}
private:
	TResourceArray<FPositionOnlyVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
};
