#include "CortoDataUploader.h"
#include "CortoDecoder.h"
#include "CortoMeshRendererComp.h"
#include "CortoLocalMeshFrame.h"
#include "CortoWebpUnifiedDecodeResult.h"

CortoDataUploader::CortoDataUploader(UCortoMeshRendererComp* rendererComponent) :
	m_localTextureFrame(nullptr), m_rendererComponent(rendererComponent), m_dataDirty(false)
{

}

bool CortoDataUploader::ForceUpload()
{
	// upload data to sceneproxy
	if (m_localMeshFrame && m_rendererComponent && m_rendererComponent->SceneProxy)
	{
		m_rendererComponent->SetMeshData(m_localMeshFrame);
		check(m_localTextureFrame && m_localTextureFrame->GetTexture() && m_localTextureFrame->GetTexture()->GetResource());
		m_rendererComponent->SetTextureData(m_localTextureFrame);
		
		m_dataDirty = false;
		return true;
	}

	m_dataDirty = true;
	return false;
}

bool CortoDataUploader::IsDataDirty() const
{
	return m_dataDirty;
}

void CortoDataUploader::MarkDataDirty()
{
	m_dataDirty = true;
}

void CortoDataUploader::Upload(const GenericDecodeResult* pCortoResult)
{
	const CortoWebpUnifiedDecodeResult* pResult = static_cast<const CortoWebpUnifiedDecodeResult*>(pCortoResult);
	pResult->Lock();
	if (pResult->DecodeSuccessful)
	{
		m_localMeshFrame = std::make_shared<CortoLocalMeshFrame>(pResult);
        
        // For some reason, mostly likely GC vs texture memory management, we cannot make new
        // CortoLocalTextureFrame, eventually new UTexture2D for every new uploading frame even
        // though CortoLocalTextureFrame did comply to FGCObject. Instead we have to update the
        // texture content to avoid memory exceeding the limit on iOS.
        if (!m_localTextureFrame)
        {
			m_localTextureFrame = std::make_shared<CortoLocalTextureFrame>(pResult);
        }
        else
        {
            m_localTextureFrame->UpdateTexture(pResult);
        }
		pResult->Unlock();
		
		ForceUpload();
	}
	else
	{
		pResult->Unlock();
	}
}

void CortoDataUploader::ReleaseLocalResource()
{
	m_localMeshFrame.reset();
	m_localTextureFrame.reset();
}
