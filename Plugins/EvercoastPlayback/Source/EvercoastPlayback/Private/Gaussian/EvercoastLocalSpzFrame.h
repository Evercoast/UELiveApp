#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Engine/Texture2D.h"
#include "UnrealEngineCompatibility.h"

class EvercoastGaussianSplatDecodeResult;
class EvercoastLocalSpzFrame : public FGCObject
{
public:
	EvercoastLocalSpzFrame(const EvercoastGaussianSplatDecodeResult* pResult);
	virtual ~EvercoastLocalSpzFrame();

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual FString GetReferencerName() const override
	{
		return TEXT("EvercoastLocalSpzFrame");
	}

	// TODO: SH textures
#if ENGINE_MAJOR_VERSION == 5
	TObjectPtr<UTexture2D> PositionTex;
	TObjectPtr<UTexture2D> ColourAlphaTex;
	TObjectPtr<UTexture2D> ScaleTex;
	TObjectPtr<UTexture2D> RotationTex;
	TObjectPtr<UTexture2D> SHCoeffTexArray[3];
	//TObjectPtr<UTexture> TransformATex;
#else
	UTexture2D* PositionTex;
	UTexture2D* ColourAlphaTex;
	//UTexture2D* TransformATex;
	UTexture2D* ScaleTex;
	UTexture2D* RotationTex;
	UTexture2D* SHCoeffTexArray[3];
#endif
	uint32_t TexDimension;
	uint32_t PointCount;

private:
	void UpdatePositionTexture(uint32_t newTextureSize, float* positionData, uint32_t pointCount);
	void UpdateColourAlphaTexture(uint32_t newTextureSize, uint8_t* colourAlphaData, uint32_t pointCount);
	void UpdateFloatColourAlphaTexture(uint32_t newTextureSize, float* floatColourAlphaData, uint32_t pointCount);
	//void UpdateTransformATexture(uint32_t newTextureSize, uint32_t* transformAData, uint32_t pointCount);
	void UpdateScaleTexture(uint32_t newTextureSize, float* scaleData, uint32_t pointCount);
	void UpdateRotationTexture(uint32_t newTextureSize, float* rotationData, uint32_t pointCount);
	void UpdateSHCoeffTexture(uint32_t newTextureSize, uint32_t pointCount, uint32_t shDegree, uint32_t* shCoeff_R_Data, uint32_t* shCoeff_G_Data, uint32_t* shCoeff_B_Data);
};