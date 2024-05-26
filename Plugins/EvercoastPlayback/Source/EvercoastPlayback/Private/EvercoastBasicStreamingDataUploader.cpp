#include "EvercoastBasicStreamingDataUploader.h"
#include "EvercoastVoxelRendererComp.h"
#include "EvercoastMVFVoxelRendererComp.h"
#include "ec_decoder_compatibility.h"
#include "EvercoastDecoder.h"
#include "EvercoastLocalVoxelFrame.h"

EvercoastBasicStreamingDataUploader::EvercoastBasicStreamingDataUploader(IVoxelRendererComponent* rendererComponent) :
	m_rendererComponent(rendererComponent), m_dataDirty(false)
{

}


bool EvercoastBasicStreamingDataUploader::ForceUpload()
{
	// upload data to sceneproxy
	if (m_localVoxelFrame && m_rendererComponent && m_rendererComponent->GetSceneProxy())
	{
		m_rendererComponent->SetVoxelData(m_localVoxelFrame);
		m_dataDirty = false;
		return true;
	}

	m_dataDirty = true;
	return false;
}

bool EvercoastBasicStreamingDataUploader::IsDataDirty() const
{
	return m_dataDirty;
}

void EvercoastBasicStreamingDataUploader::MarkDataDirty()
{
	m_dataDirty = true;
}

void EvercoastBasicStreamingDataUploader::Upload(const GenericDecodeResult* pVoxelResult)
{
	const EvercoastDecodeResult* pResult = static_cast<const EvercoastDecodeResult*>(pVoxelResult);
	if (pResult && pResult->resultFrame != InvalidHandle)
	{
		m_localVoxelFrame = std::make_shared<EvercoastLocalVoxelFrame>(pResult->resultFrame, true); // copy voxels
		ForceUpload();
	}
}

void EvercoastBasicStreamingDataUploader::ReleaseLocalResource()
{
	m_localVoxelFrame.reset();
}
