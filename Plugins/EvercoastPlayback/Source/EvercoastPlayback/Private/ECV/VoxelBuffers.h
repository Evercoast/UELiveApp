// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ResourceArray.h"

class FVoxelVertexResourceArray :
	public FResourceArrayInterface
{
public:
	FVoxelVertexResourceArray(const void* InData, uint32 InSize)
		: Data(InData)
		, Size(InSize)
	{
	}

	virtual const void* GetResourceData() const override { return nullptr; }
	virtual uint32 GetResourceDataSize() const override { return Size; }
	virtual void Discard() override { }
	virtual bool IsStatic() const override { return false; }
	virtual bool GetAllowCPUAccess() const override { return false; }
	virtual void SetAllowCPUAccess(bool bInNeedsCPUAccess) override { }

private:
	const void* Data;
	uint32 Size;
};

/** Point cloud vertex buffer that can hold an arbitrary single data type (color or position) */
class FVoxelVertexBufferBase :
	public FVertexBuffer
{
public:
	inline uint32 GetNumVerts() const
	{
		return NumVoxels;
	}

	inline FShaderResourceViewRHIRef GetBufferSRV() const
	{
		return BufferSRV;
	}

	void Update(FRHICommandListBase& RHICmdList, const void* InData, uint32 InNum)
	{
		uint32 UpdateSize = InNum * SizePerVoxel;
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 3
		auto* Indices = RHICmdList.LockBuffer(VertexBufferRHI, 0, UpdateSize, RLM_WriteOnly);
		FMemory::Memcpy(Indices,
			InData,
			UpdateSize);
		RHICmdList.UnlockBuffer(VertexBufferRHI);
#else
		auto* Indices = RHILockBuffer(VertexBufferRHI, 0, UpdateSize, RLM_WriteOnly);
		FMemory::Memcpy(Indices,
			InData,
			UpdateSize);
		RHIUnlockBuffer(VertexBufferRHI);
#endif
#else
		auto* Vertices = RHILockVertexBuffer(VertexBufferRHI, 0, UpdateSize, RLM_WriteOnly);
		FMemory::Memcpy(Vertices,
			InData,
			UpdateSize);
		RHIUnlockVertexBuffer(VertexBufferRHI);
#endif
	}

protected:
	void InitWith(FRHICommandListBase& RHICmdList, uint32 InMaxVoxels, uint32 InVoxelSize)
	{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
		InitResource(RHICmdList);
#else
		InitResource();
#endif
		NumVoxels = InMaxVoxels;
		SizePerVoxel = InVoxelSize;

		const uint32 Size = InVoxelSize * NumVoxels;
		const uint32 Stride = InVoxelSize;
		FRHIResourceCreateInfo CreateInfo(TEXT("FVoxelVertexBufferBase"));
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
		VertexBufferRHI = RHICmdList.CreateVertexBuffer(Size, BUF_Dynamic | BUF_ShaderResource, CreateInfo);
#else
		VertexBufferRHI = RHICreateVertexBuffer(Size, BUF_Dynamic | BUF_ShaderResource, CreateInfo);
#endif
	}

	uint32 NumVoxels = 0;
	uint32 SizePerVoxel = 0;
	FShaderResourceViewRHIRef BufferSRV;
};

/** Point cloud color buffer that sets the SRV too */
class FVoxelColorVertexBuffer :
	public FVoxelVertexBufferBase
{
public:
	void InitRHIWithSize(FRHICommandListBase& RHICmdList, uint32 InMaxVoxels)
	{
		const auto stride = sizeof(uint8_t) * 4;
		InitWith(RHICmdList, InMaxVoxels, stride);
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
		BufferSRV = RHICmdList.CreateShaderResourceView(VertexBufferRHI,
			FRHIViewDesc::CreateBufferSRV()
			.SetType(FRHIViewDesc::EBufferType::Typed)
			.SetStride(stride)
			.SetFormat(EPixelFormat(PF_R8G8B8A8)));
#else
		BufferSRV = RHICreateShaderResourceView(VertexBufferRHI, stride, PF_R8G8B8A8);
#endif
		// We have a full stream so we can index safely
		ColorMask = ~0;
	}

	inline uint32 GetColorMask() const
	{
		return ColorMask;
	}

private:
	uint32 ColorMask = 0;
};

/** Point cloud location buffer that sets the SRV too */
class FVoxelPositionVertexBuffer :
	public FVoxelVertexBufferBase
{
public:
	void InitRHIWithSize(FRHICommandListBase& RHICmdList, uint32 InMaxVoxels)
	{
		const auto stride = sizeof(uint16_t) * 4;
		InitWith(RHICmdList, InMaxVoxels, stride);
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
		BufferSRV = RHICmdList.CreateShaderResourceView(VertexBufferRHI,
			FRHIViewDesc::CreateBufferSRV()
			.SetType(FRHIViewDesc::EBufferType::Typed)
			.SetStride(stride)
			.SetFormat(EPixelFormat(PF_R16G16B16A16_UINT)));
#else
		BufferSRV = RHICreateShaderResourceView(VertexBufferRHI, stride, PF_R16G16B16A16_UINT);
#endif
	}
};

/**
 * We generate a index buffer for N points in the point cloud section. Each point
 * generates the vert positions using vertex id and point id (vertex id / 4)
 * by fetching the points and colors from the buffers
 */
class FVoxelIndexBuffer :
	public FIndexBuffer
{
public:
	FVoxelIndexBuffer()
		: NumPoints(0)
		, MaxPoints(0)
		, MaxIndex(0)
		, PrimitiveMod(0)
		, bIsQuadList(false)
	{
	}

	FVoxelIndexBuffer(uint32 InNumPoints)
		: NumPoints(0)
		, MaxPoints(InNumPoints)
		, MaxIndex(4 * InNumPoints)
		, PrimitiveMod(0)
		, bIsQuadList(false)
	{
	}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
#else
	virtual void InitRHI() override
#endif
	{
		check(MaxPoints > 0 && MaxIndex > 0);

		PrimitiveMod = 12;
		const uint32 Size = sizeof(uint32) * 36 * MaxPoints;
		const uint32 Stride = sizeof(uint32);
		FRHIResourceCreateInfo CreateInfo(TEXT("VoxelTriList"));
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 3
		IndexBufferRHI = RHICmdList.CreateBuffer(Size, BUF_Static | BUF_IndexBuffer, Stride, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		uint32* Indices = (uint32*)RHICmdList.LockBuffer(IndexBufferRHI, 0, Size, RLM_WriteOnly);
#else
		IndexBufferRHI = RHICreateBuffer(Size, BUF_Static | BUF_IndexBuffer, Stride, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		uint32* Indices = (uint32*)RHILockBuffer(IndexBufferRHI, 0, Size, RLM_WriteOnly);
#endif
#else
		IndexBufferRHI = RHICreateIndexBuffer(Stride, Size, BUF_Static | BUF_IndexBuffer, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		uint32* Indices = (uint32*)RHILockIndexBuffer(IndexBufferRHI, 0, Size, RLM_WriteOnly);
#endif
		uint32 c = 0;
		for (uint32 SpriteIndex = 0; SpriteIndex < MaxPoints; ++SpriteIndex)
		{
			const auto o = SpriteIndex * 8 * 6;
			Indices[c++] = o + 0;
			Indices[c++] = o + 2;
			Indices[c++] = o + 1;

			Indices[c++] = o + 1;
			Indices[c++] = o + 2;
			Indices[c++] = o + 3;
			
			Indices[c++] = o + 2 + 8;
			Indices[c++] = o + 7 + 8;
			Indices[c++] = o + 3 + 8;
			
			Indices[c++] = o + 2 + 8;
			Indices[c++] = o + 6 + 8;
			Indices[c++] = o + 7 + 8;
			
			Indices[c++] = o + 1 + 16;
			Indices[c++] = o + 3 + 16;
			Indices[c++] = o + 7 + 16;
			
			Indices[c++] = o + 1 + 16;
			Indices[c++] = o + 7 + 16;
			Indices[c++] = o + 5 + 16;
			
			Indices[c++] = o + 6 + 24;
			Indices[c++] = o + 4 + 24;
			Indices[c++] = o + 7 + 24;
			
			Indices[c++] = o + 7 + 24;
			Indices[c++] = o + 4 + 24;
			Indices[c++] = o + 5 + 24;
			
			Indices[c++] = o + 0 + 32;
			Indices[c++] = o + 1 + 32;
			Indices[c++] = o + 4 + 32;
			
			Indices[c++] = o + 1 + 32;
			Indices[c++] = o + 5 + 32;
			Indices[c++] = o + 4 + 32;
			
			Indices[c++] = o + 2 + 40;
			Indices[c++] = o + 4 + 40;
			Indices[c++] = o + 6 + 40;
			
			Indices[c++] = o + 0 + 40;
			Indices[c++] = o + 4 + 40;
			Indices[c++] = o + 2 + 40;
		}
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 3
		RHICmdList.UnlockBuffer(IndexBufferRHI);
#else
		RHIUnlockBuffer(IndexBufferRHI);
#endif
#else
		RHIUnlockIndexBuffer(IndexBufferRHI);
#endif
	}

	void InitRHIWithSize(FRHICommandListBase& RHICmdList, uint32 InNumPoints)
	{
		MaxPoints = InNumPoints;
		MaxIndex = (8 * 6 * InNumPoints) - 1;
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
		InitResource(RHICmdList);
#else
		InitResource();
#endif
	}

	void SetNumPoints(uint32 InNumPoints)
	{
		NumPoints = InNumPoints;
		if (NumPoints > MaxPoints) 
		{
			NumPoints = MaxPoints;
		}
		MaxIndex = (8 * 6 * NumPoints) - 1;
	}

	inline bool IsQuadList() const
	{
		return bIsQuadList;
	}

	inline bool IsTriList() const
	{
		return !IsQuadList();
	}

	inline uint32 GetNumPrimitives() const
	{
		return NumPoints * PrimitiveMod;
	}

	inline uint32 GetMaxIndex() const
	{
		return MaxIndex;
	}

private:
	uint32 NumPoints;
	uint32 MaxPoints;
	uint32 MaxIndex;
	uint32 PrimitiveMod;
	bool bIsQuadList;
};
