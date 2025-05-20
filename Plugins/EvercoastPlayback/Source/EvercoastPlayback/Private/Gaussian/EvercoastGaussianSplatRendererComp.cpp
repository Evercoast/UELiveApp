#include "Gaussian/EvercoastGaussianSplatRendererComp.h"
#include "Gaussian/EvercoastGaussianSplatUploader.h"
#include "Gaussian/EvercoastLocalSpzFrame.h"
#include "Engine/Texture.h"
#include "NiagaraFunctionLibrary.h"

UEvercoastGaussianSplatRendererComp::UEvercoastGaussianSplatRendererComp(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	m_gaussianUploader = std::make_shared<EvercoastGaussianSplatUploader>(this);
}

void UEvercoastGaussianSplatRendererComp::SetGaussianSplatData(std::shared_ptr<EvercoastLocalSpzFrame> spzFrame)
{
	spzFrame->PositionTex->Filter = TextureFilter::TF_Nearest;
	spzFrame->PositionTex->AddressX = TextureAddress::TA_Clamp;
	spzFrame->PositionTex->AddressY = TextureAddress::TA_Clamp;

	spzFrame->ColourAlphaTex->Filter = TextureFilter::TF_Nearest;
	spzFrame->ColourAlphaTex->AddressX = TextureAddress::TA_Clamp;
	spzFrame->ColourAlphaTex->AddressY = TextureAddress::TA_Clamp;

	spzFrame->ScaleTex->Filter = TextureFilter::TF_Nearest;
	spzFrame->ScaleTex->AddressX = TextureAddress::TA_Clamp;
	spzFrame->ScaleTex->AddressY = TextureAddress::TA_Clamp;

	spzFrame->RotationTex->Filter = TextureFilter::TF_Nearest;
	spzFrame->RotationTex->AddressX = TextureAddress::TA_Clamp;
	spzFrame->RotationTex->AddressY = TextureAddress::TA_Clamp;

	for (int i = 0; i < 3; ++i)
	{
		spzFrame->SHCoeffTexArray[i]->Filter = TextureFilter::TF_Nearest;
		spzFrame->SHCoeffTexArray[i]->AddressX = TextureAddress::TA_Clamp;
		spzFrame->SHCoeffTexArray[i]->AddressY = TextureAddress::TA_Clamp;
	}


	// Feed spzFrame->PositionTex to Niagara FX
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	this->SetVariableInt(TEXT("PosTexture_Size"), spzFrame->TexDimension);
	this->SetVariableInt(TEXT("PointCount"), spzFrame->PointCount);
#else
	this->SetNiagaraVariableInt(TEXT("PosTexture_Size"), spzFrame->TexDimension);
	this->SetNiagaraVariableInt(TEXT("PointCount"), spzFrame->PointCount);
#endif
	

	UNiagaraFunctionLibrary::SetTextureObject(this, TEXT("PosTextureSampler"), spzFrame->PositionTex);
	UNiagaraFunctionLibrary::SetTextureObject(this, TEXT("ColourAlphaTextureSampler"), spzFrame->ColourAlphaTex);
	UNiagaraFunctionLibrary::SetTextureObject(this, TEXT("ScaleTextureSampler"), spzFrame->ScaleTex);
	UNiagaraFunctionLibrary::SetTextureObject(this, TEXT("RotationTextureSampler"), spzFrame->RotationTex);
	UNiagaraFunctionLibrary::SetTextureObject(this, TEXT("SHCoeff_R_TextureSampler"), spzFrame->SHCoeffTexArray[0]);
	UNiagaraFunctionLibrary::SetTextureObject(this, TEXT("SHCoeff_G_TextureSampler"), spzFrame->SHCoeffTexArray[1]);
	UNiagaraFunctionLibrary::SetTextureObject(this, TEXT("SHCoeff_B_TextureSampler"), spzFrame->SHCoeffTexArray[2]);
}

std::shared_ptr<IEvercoastStreamingDataUploader> UEvercoastGaussianSplatRendererComp::GetDataUploader() const
{
	return m_gaussianUploader;
}