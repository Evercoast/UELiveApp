#include "EvercoastLocalSpzFrame.h"
#include "Gaussian/EvercoastGaussianSplatDecoder.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "TextureResource.h"

EvercoastLocalSpzFrame::EvercoastLocalSpzFrame(const EvercoastGaussianSplatDecodeResult* pResult) :
	PositionTex(nullptr),
	ColourAlphaTex(nullptr),
	//TransformATex(nullptr),
	ScaleTex(nullptr),
	RotationTex(nullptr),
	TexDimension(0),
	PointCount(0)
{
	

	UpdatePositionTexture(pResult->textureSize, pResult->positions, pResult->pointCount);
	UpdateFloatColourAlphaTexture(pResult->textureSize, pResult->floatColourAlphas, pResult->pointCount);
	UpdateScaleTexture(pResult->textureSize, pResult->scales, pResult->pointCount);
	UpdateRotationTexture(pResult->textureSize, pResult->rotationQuats, pResult->pointCount);

	for(int i = 0; i < 3; ++i)
		SHCoeffTexArray[i] = nullptr;
	UpdateSHCoeffTexture(pResult->textureSize, pResult->pointCount, pResult->shDegree, pResult->shCoeffs_R, pResult->shCoeffs_G, pResult->shCoeffs_B);

	PointCount = pResult->pointCount;
}


EvercoastLocalSpzFrame::~EvercoastLocalSpzFrame()
{
	if (PositionTex)
	{
		PositionTex->ReleaseResource();
		PositionTex->ConditionalBeginDestroy();
		PositionTex = nullptr;
	}

	if (ColourAlphaTex)
	{
		ColourAlphaTex->ReleaseResource();
		ColourAlphaTex->ConditionalBeginDestroy();
		ColourAlphaTex = nullptr;
	}

	/*
	if (TransformATex)
	{
		TransformATex->ReleaseResource();
		TransformATex = nullptr;
	}
	*/
	if (ScaleTex)
	{
		ScaleTex->ReleaseResource();
		ScaleTex->ConditionalBeginDestroy();
		ScaleTex = nullptr;
	}

	if (RotationTex)
	{
		RotationTex->ReleaseResource();
		RotationTex->ConditionalBeginDestroy();
		RotationTex = nullptr;
	}

	for (int i = 0; i < 3; ++i)
	{
		if (SHCoeffTexArray[i])
		{
			SHCoeffTexArray[i]->ReleaseResource();
			SHCoeffTexArray[i]->ConditionalBeginDestroy();
			SHCoeffTexArray[i] = nullptr;
		}
	}

}

template<typename T>
static UTexture2D* UpdateTypedTextureData(UTexture2D* existingTexture, uint32_t newTextureSize, T* typedData, uint32_t pointCount, bool isSRGB)
{
	UTexture2D* pTexture;

	// FIXME: save texture creation!
	if (!existingTexture || existingTexture->GetSurfaceWidth() < newTextureSize || existingTexture->GetSurfaceHeight() < newTextureSize)
	{
		if (existingTexture)
		{
			existingTexture->ReleaseResource();
			existingTexture = nullptr;
		}

		if (std::is_same_v<T, float>)
		{
			pTexture = UTexture2D::CreateTransient(newTextureSize, newTextureSize, EPixelFormat::PF_A32B32G32R32F);
			
		}
		else if (std::is_same_v<T, uint32_t>)
		{
			pTexture = UTexture2D::CreateTransient(newTextureSize, newTextureSize, EPixelFormat::PF_R32G32B32A32_UINT);
		}
		else
		{
			pTexture = UTexture2D::CreateTransient(newTextureSize, newTextureSize, EPixelFormat::PF_R8G8B8A8);
		}

		pTexture->NeverStream = true;
		pTexture->SRGB = isSRGB;
		pTexture->UpdateResource();
	}
	else
	{
		
		pTexture = existingTexture;
	}


#if ENGINE_MAJOR_VERSION == 5
	FTexture2DMipMap& Mip0 = pTexture->GetPlatformData()->Mips[0];
#else
	FTexture2DMipMap& Mip0 = pTexture->PlatformData->Mips[0];
#endif
	void* TextureData = Mip0.BulkData.Lock(LOCK_READ_WRITE);

	const int32 PixelStride = (int32)(sizeof(T) * 4);
	size_t bytesToCopy = SIZE_T(pointCount * PixelStride);
	FMemory::Memcpy(TextureData, typedData, bytesToCopy);
	// clean the unfilled data
	int32_t leftBytes = newTextureSize * newTextureSize * PixelStride - bytesToCopy;
	if (leftBytes > 0)
	{
		FMemory::Memzero((uint8_t*)TextureData + bytesToCopy, SIZE_T(leftBytes));
	}

	Mip0.BulkData.Unlock();

	// need update GPU resource after updating CPU side data
	pTexture->UpdateResource();

	return pTexture;
}

void EvercoastLocalSpzFrame::UpdatePositionTexture(uint32_t newTextureSize, float* positionData, uint32_t pointCount)
{
	PositionTex = UpdateTypedTextureData(PositionTex, newTextureSize, positionData, pointCount, false);
	TexDimension = PositionTex->GetSurfaceWidth();
}

void EvercoastLocalSpzFrame::UpdateColourAlphaTexture(uint32_t newTextureSize, uint8_t* colourAlphaData, uint32_t pointCount)
{
	ColourAlphaTex = UpdateTypedTextureData(ColourAlphaTex, newTextureSize, colourAlphaData, pointCount, true);
}

void EvercoastLocalSpzFrame::UpdateFloatColourAlphaTexture(uint32_t newTextureSize, float* floatColourAlphaData, uint32_t pointCount)
{
	ColourAlphaTex = UpdateTypedTextureData(ColourAlphaTex, newTextureSize, floatColourAlphaData, pointCount, true);
}

void EvercoastLocalSpzFrame::UpdateScaleTexture(uint32_t newTextureSize, float* scaleData, uint32_t pointCount)
{
	ScaleTex = UpdateTypedTextureData(ScaleTex, newTextureSize, scaleData, pointCount, false);
}

void EvercoastLocalSpzFrame::UpdateRotationTexture(uint32_t newTextureSize, float* rotationData, uint32_t pointCount)
{
	RotationTex = UpdateTypedTextureData(RotationTex, newTextureSize, rotationData, pointCount, false);
}

/*
void EvercoastLocalSpzFrame::UpdateTransformATexture(uint32_t newTextureSize, uint32_t* transformAData, uint32_t pointCount)
{
	TransformATex = UpdateTypedTextureData(TransformATex, newTextureSize, transformAData, pointCount);
}
*/

void EvercoastLocalSpzFrame::UpdateSHCoeffTexture(uint32_t newTextureSize, uint32_t pointCount, uint32_t shDegree, 
	uint32_t* shCoeff_R_Data, uint32_t* shCoeff_G_Data, uint32_t* shCoeff_B_Data)
{
	SHCoeffTexArray[0] = UpdateTypedTextureData(SHCoeffTexArray[0], newTextureSize, shCoeff_R_Data, pointCount, false);
	SHCoeffTexArray[1] = UpdateTypedTextureData(SHCoeffTexArray[1], newTextureSize, shCoeff_G_Data, pointCount, false);
	SHCoeffTexArray[2] = UpdateTypedTextureData(SHCoeffTexArray[2], newTextureSize, shCoeff_B_Data, pointCount, false);
}



void EvercoastLocalSpzFrame::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (PositionTex)
		Collector.AddReferencedObject(PositionTex);

	if (ColourAlphaTex)
		Collector.AddReferencedObject(ColourAlphaTex);

	if (ScaleTex)
		Collector.AddReferencedObject(ScaleTex);

	if (RotationTex)
		Collector.AddReferencedObject(RotationTex);
	/*
	if (TransformATex)
		Collector.AddReferencedObject(TransformATex);
	*/

	for (int i = 0; i < 3; ++i)
	{
		if (SHCoeffTexArray[i])
		{
			Collector.AddReferencedObject(SHCoeffTexArray[i]);
		}
	}
}