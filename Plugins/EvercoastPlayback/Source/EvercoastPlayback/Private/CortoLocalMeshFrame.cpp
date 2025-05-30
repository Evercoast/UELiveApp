#include "CortoLocalMeshFrame.h"
#include "EvercoastVoxelDecoder.h"
#include "CortoWebpUnifiedDecodeResult.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "RenderingThread.h"

#if PLATFORM_ANDROID
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2
#include "DataDrivenShaderPlatformInfo.h"
#else
#include "RHIDefinitions.h"
#endif
#endif

// NOTE:
// We need to keep this local mesh frame class to do the unit and coordinate system conversion
// Once figured out how to do it on-the-fly in a vertex shader, we can omit those memcpy and just use
// CorotoDecodeResult instead.
CortoLocalMeshFrame::CortoLocalMeshFrame(const CortoWebpUnifiedDecodeResult* pResult) :
	m_vertexCount(pResult->meshResult->VertexCount),
	m_triangleCount(pResult->meshResult->TriangleCount)
{
	// triangles
	m_indexBuffer.SetNumUninitialized(GetIndexCount());
	FMemory::Memcpy(m_indexBuffer.GetData(), pResult->meshResult->IndexBuffer.data(), sizeof(uint32_t) * GetIndexCount());

	// uv0
	m_uvBuffer.SetNumUninitialized(GetVertexCount());
	FMemory::Memcpy(m_uvBuffer.GetData(), pResult->meshResult->UVBuffer.data(), sizeof(FVector2f) * GetVertexCount());

	// position needs to be scaled (x100) and swap y<->z so there's no memcpy
	m_positionBuffer.SetNumUninitialized(GetVertexCount());
	//FMemory::Memcpy(m_positionBuffer.GetData(), pResult->meshResult->PositionBuffer, sizeof(FVector) * GetVertexCount());
	FVector3f* pDst = m_positionBuffer.GetData();
	const FVector3f* pSrc = pResult->meshResult->PositionBuffer.data();;
	
	ParallelFor(m_vertexCount, [&](int32 k)
		{
			const FVector3f& SrcVertexPos = *(pSrc + k);
			FVector3f& DstVertexPos = *(pDst + k);

			DstVertexPos.X = SrcVertexPos.X * 100.0f;
			DstVertexPos.Y = SrcVertexPos.Z * 100.0f;
			DstVertexPos.Z = SrcVertexPos.Y * 100.0f;
		});

	if (pResult->meshResult->HasNormals())
	{
		m_normalBuffer.SetNumUninitialized(GetVertexCount());

		FVector3f* pNormalDst = m_normalBuffer.GetData();
		const FVector3f* pNormalSrc = pResult->meshResult->NormalBuffer.data();;

		ParallelFor(m_vertexCount, [&](int32 k)
			{
				const FVector3f& SrcVertexNormal = *(pNormalSrc + k);
				FVector3f& DstVertexNormal = *(pNormalDst + k);

				DstVertexNormal.X = SrcVertexNormal.X;
				DstVertexNormal.Y = SrcVertexNormal.Z;
				DstVertexNormal.Z = SrcVertexNormal.Y;
			});
	}
	else
	{
		m_normalBuffer.Empty();
	}

	m_bounds = FBoxSphereBounds3f(m_positionBuffer.GetData(), GetVertexCount());
	if (m_bounds.BoxExtent.ContainsNaN() || m_bounds.Origin.ContainsNaN())
	{
		UE_LOG(EvercoastVoxelDecoderLog, Error, TEXT("Bounds NaN found! Probably due to vertex data corruption with threading."));

		m_bounds = FBoxSphereBounds3f(FSphere3f(FVector3f::ZeroVector, 100.0f));
	}
}

FBoxSphereBounds3f CortoLocalMeshFrame::GetBounds() const
{
	return m_bounds;
}


CortoLocalTextureFrame::CortoLocalTextureFrame(const CortoWebpUnifiedDecodeResult* pResult) :
    FGCObject(),
	m_localTexture(nullptr),
	m_needsSwizzle(false)
{
    UpdateTexture(pResult);
}

void CortoLocalTextureFrame::UpdateTexture(const CortoWebpUnifiedDecodeResult* pResult)
{
	if (pResult->imgResult->IsValid())
	{
		// use webp image
		auto Width = pResult->imgResult->Width;
		auto Height = pResult->imgResult->Height;
		auto BitPerPixel = pResult->imgResult->BitPerPixel;

		if (!m_localTexture || m_localTexture->GetSurfaceWidth() != Width || m_localTexture->GetSurfaceHeight() != Height)
		{
			UE_LOG(EvercoastVoxelDecoderLog, Warning, TEXT("Image LOD level changed."));
			if (m_localTexture)
			{
				m_localTexture->ReleaseResource();
				m_localTexture = nullptr;
			}
			m_localTexture = UTexture2D::CreateTransient(Width, Height, EPixelFormat::PF_B8G8R8A8);
			m_localTexture->UpdateResource();
		}

		auto Texture2D = static_cast<UTexture2D*>(m_localTexture);
#if ENGINE_MAJOR_VERSION == 5
		FTexture2DMipMap& Mip0 = Texture2D->GetPlatformData()->Mips[0];
#else
		FTexture2DMipMap& Mip0 = Texture2D->PlatformData->Mips[0];
#endif
		void* TextureData = Mip0.BulkData.Lock(LOCK_READ_WRITE);

		const int32 PixelStride = (int32)(BitPerPixel / 8);
		FMemory::Memcpy(TextureData, pResult->imgResult->RawTexelBuffer, SIZE_T(Width * Height * PixelStride));

		Mip0.BulkData.Unlock();

		// need update GPU resource after updating CPU side data
		m_localTexture->UpdateResource();

		m_needsSwizzle = false;
	}
	else if (pResult->videoTextureResult)
	{
		// use video texture
		auto pTexture = pResult->videoTextureResult;
		auto copyPromise = MakeShared<TPromise<void>, ESPMode::ThreadSafe>();
		auto future = copyPromise->GetFuture();

		// getting ready to make a copy
		int32 width = pTexture->GetSurfaceWidth();
		int32 height = pTexture->GetSurfaceHeight();
		if (!m_localTexture || m_localTexture->GetSurfaceWidth() != width || m_localTexture->GetSurfaceHeight() != height)
		{
			UE_LOG(EvercoastVoxelDecoderLog, Warning, TEXT("Video LOD level changed."));
			if (m_localTexture)
			{
				m_localTexture->ReleaseResource();
				m_localTexture = nullptr;
			}
			m_localTexture = UTexture2D::CreateTransient(width, height, EPixelFormat::PF_B8G8R8A8);
			m_localTexture->UpdateResource();
		}

		ENQUEUE_RENDER_COMMAND(CortoLocalTextureFrame_CopyTexture)(
			[srcTex = pTexture, mainTex = m_localTexture, self = this, promise = copyPromise](FRHICommandListImmediate& RHICmdList)
			{
				auto targetRHIRes = mainTex->GetResource();
				auto srcRHIRes = srcTex->GetResource();

				check(srcRHIRes);

				FTextureRHIRef targetRHI = targetRHIRes->TextureRHI;
				FTextureRHIRef srcRHI = srcRHIRes->TextureRHI;
				RHICmdList.Transition(FRHITransitionInfo(srcRHI, ERHIAccess::SRVMask, ERHIAccess::CopySrc));
				RHICmdList.Transition(FRHITransitionInfo(targetRHI, ERHIAccess::SRVMask, ERHIAccess::CopyDest));
				RHICmdList.CopyTexture(srcTex->GetResource()->TextureRHI, mainTex->GetResource()->TextureRHI, FRHICopyTextureInfo());
				RHICmdList.Transition(FRHITransitionInfo(targetRHI, ERHIAccess::CopyDest, ERHIAccess::SRVMask));
				RHICmdList.Transition(FRHITransitionInfo(srcRHI, ERHIAccess::CopySrc, ERHIAccess::SRVMask));

				promise->SetValue();
			});

		future.Get();
#if PLATFORM_ANDROID

	if (IsAndroidOpenGLESPlatform(GMaxRHIShaderPlatform))
		m_needsSwizzle = true;
	else
		m_needsSwizzle = false;

#else
		m_needsSwizzle = false;
#endif
	}
	else
	{
		// wrong
		UE_LOG(EvercoastVoxelDecoderLog, Error, TEXT("WebP image result or video texture result are both invalid. Unable to update decode image result"));
	}
	
}

CortoLocalTextureFrame::~CortoLocalTextureFrame()
{
	if (m_localTexture)
	{
		// Release only when has the ownership
		if (m_localTexture && m_localTexture->IsValidLowLevel())
			m_localTexture->ReleaseResource();
		m_localTexture = nullptr;
	}
}

void CortoLocalTextureFrame::AddReferencedObjects(FReferenceCollector& Collector)
{
    if (m_localTexture)
        Collector.AddReferencedObject(m_localTexture);
}

