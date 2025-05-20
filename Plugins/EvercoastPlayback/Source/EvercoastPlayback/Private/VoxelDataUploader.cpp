#include "VoxelDataUploader.h"
#include "EvercoastVoxelRendererComp.h"
#include "EvercoastMVFVoxelRendererComp.h"
#include "ec_decoder_compatibility.h"
#include "EvercoastVoxelDecoder.h"
#include "EvercoastLocalVoxelFrame.h"

VoxelDataUploader::VoxelDataUploader(IVoxelRendererComponent* rendererComponent) :
	m_rendererComponent(rendererComponent), m_dataDirty(false), m_lastUploadedFrameIndex(-1)
{

}


bool VoxelDataUploader::ForceUpload()
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

bool VoxelDataUploader::IsDataDirty() const
{
	return m_dataDirty;
}

void VoxelDataUploader::MarkDataDirty()
{
	m_dataDirty = true;
}

void VoxelDataUploader::Upload(const GenericDecodeResult* pVoxelResult)
{
	const EvercoastVoxelDecodeResult* pResult = static_cast<const EvercoastVoxelDecodeResult*>(pVoxelResult);
	if (pResult && pResult->resultFrame != InvalidHandle && pResult->frameIndex != m_lastUploadedFrameIndex)
	{
		if (!m_localVoxelFrame || !m_localVoxelFrame->ContainsVoxelFrame(pResult->resultFrame))
		{
			m_localVoxelFrame = std::make_shared<EvercoastLocalVoxelFrame>(pResult->resultFrame, true); // copy voxels
			ForceUpload();

			m_lastUploadedFrameIndex = pResult->frameIndex;
		}
	}
}

void VoxelDataUploader::ReleaseLocalResource()
{
	m_localVoxelFrame.reset();
	m_lastUploadedFrameIndex = -1;
}
