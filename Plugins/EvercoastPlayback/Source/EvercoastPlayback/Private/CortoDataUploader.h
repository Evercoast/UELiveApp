#pragma once
#include <memory>
#include "EvercoastStreamingDataUploader.h"

struct CortoLocalMeshFrame;
class CortoLocalTextureFrame;
class UCortoMeshRendererComp;
class CortoDataUploader : public IEvercoastStreamingDataUploader
{
public:
	CortoDataUploader(UCortoMeshRendererComp* rendererComponent);
	virtual ~CortoDataUploader() = default;
	virtual void Upload(const GenericDecodeResult* cortoDecodeResult) override;
	virtual bool IsDataDirty() const override;
	virtual bool ForceUpload() override;
	virtual void MarkDataDirty() override;
	virtual void ReleaseLocalResource() override;
private:

	std::shared_ptr<CortoLocalMeshFrame> m_localMeshFrame;
	std::shared_ptr<CortoLocalTextureFrame> m_localTextureFrame;
	UCortoMeshRendererComp* m_rendererComponent;
	bool m_dataDirty;
	int m_lastUploadedFrameIndex;
};
