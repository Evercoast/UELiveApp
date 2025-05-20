#include "Gaussian/EvercoastGaussianSplatComputeUploader.h"
#include "Gaussian/EvercoastGaussianSplatComputeComponent.h"
#include "Gaussian/EvercoastGaussianSplatPassthroughResult.h"

EvercoastGaussianSplatComputeUploader::EvercoastGaussianSplatComputeUploader(UEvercoastGaussianSplatComputeComponent* rendererComponent) :
    m_rendererComponent(rendererComponent),
    m_dataDirty(false),
    m_lastUploadedFrameIndex(-1)
{

}

bool EvercoastGaussianSplatComputeUploader::ForceUpload()
{
    // TODO: upload data to sceneproxy
    if (m_rendererComponent)
    {
        m_rendererComponent->SetGaussianSplatData(m_pCopiedResult);
        m_dataDirty = false;
        return true;
    }
    

    m_dataDirty = true;
    return false;
}

bool EvercoastGaussianSplatComputeUploader::IsDataDirty() const
{
    return m_dataDirty;
}

void EvercoastGaussianSplatComputeUploader::MarkDataDirty()
{
    m_dataDirty = true;
}

void EvercoastGaussianSplatComputeUploader::Upload(const GenericDecodeResult* pSpzResult)
{
    const EvercoastGaussianSplatPassthroughResult* resultRawPtr = static_cast<const EvercoastGaussianSplatPassthroughResult*>(pSpzResult);
    // Copy here because we don't have the access to manage the cache content in ResultCache
    m_pCopiedResult = std::make_shared<EvercoastGaussianSplatPassthroughResult>(*resultRawPtr);
    
    if (m_pCopiedResult->DecodeSuccessful && m_lastUploadedFrameIndex != m_pCopiedResult->frameIndex)
    {
        ForceUpload();
        
        m_lastUploadedFrameIndex = m_pCopiedResult->frameIndex;
        m_pCopiedResult.reset();
    }
}

void EvercoastGaussianSplatComputeUploader::ReleaseLocalResource()
{
    if (m_pCopiedResult)
    {
        m_pCopiedResult->InvalidateResult();
        m_pCopiedResult.reset();
    }


    m_lastUploadedFrameIndex = -1;
}