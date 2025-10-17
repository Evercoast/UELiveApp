#pragma once
#include <memory>
#include "EvercoastStreamingDataUploader.h"

class UEvercoastGaussianSplatCSRendererComp;
class EvercoastGaussianSplatCSResult;
class EvercoastGaussianSplatCSUploader : public IEvercoastStreamingDataUploader
{
public:
    EvercoastGaussianSplatCSUploader(UEvercoastGaussianSplatCSRendererComp* rendererComponent);
    virtual ~EvercoastGaussianSplatCSUploader() = default;
    virtual void Upload(const GenericDecodeResult* spzDecodeResult) override;
    virtual bool IsDataDirty() const override;
    virtual bool ForceUpload() override;
    virtual void MarkDataDirty() override;
    virtual void ReleaseLocalResource() override;
private:

    UEvercoastGaussianSplatCSRendererComp* m_rendererComponent;
    std::shared_ptr<EvercoastGaussianSplatCSResult> m_pCopiedResult;
    bool m_dataDirty;
    int m_lastUploadedFrameIndex;
};