#pragma once
#include <memory>
#include "EvercoastStreamingDataUploader.h"

class UEvercoastGaussianSplatComputeComponent;
class EvercoastGaussianSplatPassthroughResult;
class EvercoastGaussianSplatComputeUploader : public IEvercoastStreamingDataUploader
{
public:
    EvercoastGaussianSplatComputeUploader(UEvercoastGaussianSplatComputeComponent* rendererComponent);
    virtual ~EvercoastGaussianSplatComputeUploader() = default;
    virtual void Upload(const GenericDecodeResult* spzDecodeResult) override;
    virtual bool IsDataDirty() const override;
    virtual bool ForceUpload() override;
    virtual void MarkDataDirty() override;
    virtual void ReleaseLocalResource() override;
private:

    UEvercoastGaussianSplatComputeComponent* m_rendererComponent;
    std::shared_ptr<EvercoastGaussianSplatPassthroughResult> m_pCopiedResult;
    bool m_dataDirty;
    int m_lastUploadedFrameIndex;
};