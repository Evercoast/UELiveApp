#include "Gaussian/EvercoastGaussianSplatCSUploader.h"
#include "Gaussian/EvercoastGaussianSplatCSRendererComp.h"
#include "Gaussian/EvercoastGaussianSplatCSResult.h"

EvercoastGaussianSplatCSUploader::EvercoastGaussianSplatCSUploader(UEvercoastGaussianSplatCSRendererComp* rendererComponent) :
    m_rendererComponent(rendererComponent),
    m_dataDirty(false),
    m_lastUploadedFrameIndex(-1)
{

}

bool EvercoastGaussianSplatCSUploader::ForceUpload()
{
    if (m_rendererComponent)
    {
        m_rendererComponent->SetGaussianSplatData(m_pCopiedResult);
        m_dataDirty = false;
        return true;
    }
    

    m_dataDirty = true;
    return false;
}

bool EvercoastGaussianSplatCSUploader::IsDataDirty() const
{
    return m_dataDirty;
}

void EvercoastGaussianSplatCSUploader::MarkDataDirty()
{
    m_dataDirty = true;
}

void EvercoastGaussianSplatCSUploader::Upload(const GenericDecodeResult* pSpzResult)
{
    const EvercoastGaussianSplatCSResult* resultRawPtr = static_cast<const EvercoastGaussianSplatCSResult*>(pSpzResult);

    if (!m_pCopiedResult || m_pCopiedResult->frameIndex != resultRawPtr->frameIndex)
    {
        // Copy here because we don't have the access to manage the cache content in ResultCache
        m_pCopiedResult = std::make_shared<EvercoastGaussianSplatCSResult>(*resultRawPtr);
    }
    
    if (m_pCopiedResult->DecodeSuccessful && m_lastUploadedFrameIndex != m_pCopiedResult->frameIndex)
    {
        ForceUpload();
        
        m_lastUploadedFrameIndex = m_pCopiedResult->frameIndex;
        m_pCopiedResult.reset();
    }
}

void EvercoastGaussianSplatCSUploader::ReleaseLocalResource()
{
    if (m_pCopiedResult)
    {
        m_pCopiedResult->InvalidateResult();
        m_pCopiedResult.reset();
    }


    m_lastUploadedFrameIndex = -1;
}