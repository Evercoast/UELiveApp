#pragma once

#include <memory>
#include "EvercoastStreamingDataUploader.h"
#include "GenericDecoder.h"

class FEvercoastVoxelSceneProxy;
struct EvercoastLocalVoxelFrame;

class IVoxelRendererComponent;
class UEvercoastVoxelRendererComp;
class UEvercoastMVFVoxelRendererComp;
class EVERCOASTPLAYBACK_API VoxelDataUploader : public IEvercoastStreamingDataUploader
{
public:
	VoxelDataUploader(IVoxelRendererComponent* rendererComponent);
	virtual ~VoxelDataUploader() = default;
	virtual void Upload(const GenericDecodeResult* voxelDecodeResult);
	virtual bool IsDataDirty() const;
	virtual bool ForceUpload();
	virtual void MarkDataDirty();
	virtual void ReleaseLocalResource();
private:
	
	std::shared_ptr<EvercoastLocalVoxelFrame> m_localVoxelFrame;
	IVoxelRendererComponent* m_rendererComponent;
	bool m_dataDirty;
	int m_lastUploadedFrameIndex;
};