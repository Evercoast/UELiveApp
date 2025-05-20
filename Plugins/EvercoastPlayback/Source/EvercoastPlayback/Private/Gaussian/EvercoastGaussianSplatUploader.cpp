#include "Gaussian/EvercoastGaussianSplatUploader.h"
#include "Gaussian/EvercoastGaussianSplatDecoder.h"
#include "Gaussian/EvercoastLocalSpzFrame.h"
#include "Gaussian/EvercoastGaussianSplatRendererComp.h"


EvercoastGaussianSplatUploader::EvercoastGaussianSplatUploader(UEvercoastGaussianSplatRendererComp* rendererComponent) :
	m_rendererComponent(rendererComponent),
	m_dataDirty(false),
	m_lastUploadedFrameIndex(-1)
{

}

bool EvercoastGaussianSplatUploader::ForceUpload()
{
	// upload data to sceneproxy
	if (m_localSpzFrame && m_rendererComponent && m_rendererComponent->SceneProxy)
	{
		m_rendererComponent->SetGaussianSplatData(m_localSpzFrame);
		m_dataDirty = false;
		return true;
	}

	m_dataDirty = true;
	return false;
}

bool EvercoastGaussianSplatUploader::IsDataDirty() const
{
	return m_dataDirty;
}

void EvercoastGaussianSplatUploader::MarkDataDirty()
{
	m_dataDirty = true;
}

void EvercoastGaussianSplatUploader::Upload(const GenericDecodeResult* pSpzResult)
{
	const EvercoastGaussianSplatDecodeResult* pResult = static_cast<const EvercoastGaussianSplatDecodeResult*>(pSpzResult);
	if (pResult->DecodeSuccessful && m_lastUploadedFrameIndex != pResult->frameIndex)
	{
		m_localSpzFrame = std::make_shared<EvercoastLocalSpzFrame>(pResult);

		ForceUpload();

		m_lastUploadedFrameIndex = pResult->frameIndex;
	}
}

void EvercoastGaussianSplatUploader::ReleaseLocalResource()
{
	m_localSpzFrame.reset();
	m_lastUploadedFrameIndex = -1;
}