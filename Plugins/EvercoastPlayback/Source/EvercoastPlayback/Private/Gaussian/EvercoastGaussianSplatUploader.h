#pragma once

#include <memory>
#include "EvercoastStreamingDataUploader.h"

class UEvercoastGaussianSplatRendererComp;
class EvercoastLocalSpzFrame;
class EvercoastGaussianSplatUploader : public IEvercoastStreamingDataUploader
{
public:
	EvercoastGaussianSplatUploader(UEvercoastGaussianSplatRendererComp* rendererComponent);
	virtual ~EvercoastGaussianSplatUploader() = default;
	virtual void Upload(const GenericDecodeResult* spzDecodeResult) override;
	virtual bool IsDataDirty() const override;
	virtual bool ForceUpload() override;
	virtual void MarkDataDirty() override;
	virtual void ReleaseLocalResource() override;
private:

	std::shared_ptr<EvercoastLocalSpzFrame> m_localSpzFrame;
	UEvercoastGaussianSplatRendererComp* m_rendererComponent;
	bool m_dataDirty;
	int m_lastUploadedFrameIndex;
};

