#pragma once

#include "CoreMinimal.h"
#include <memory>

struct EvercoastLocalVoxelFrame;
class FPrimitiveSceneProxy;
class IEvercoastStreamingDataUploader;
class UMaterialInterface;
class IVoxelRendererComponent
{
public:
	virtual void SetVoxelData(std::shared_ptr<EvercoastLocalVoxelFrame> localVoxelFrame) = 0;
	virtual FPrimitiveSceneProxy* GetSceneProxy() const = 0;
	virtual std::shared_ptr<IEvercoastStreamingDataUploader> GetVoxelDataUploader() const = 0;
	virtual void SetVoxelMaterial(UMaterialInterface* newMaterial) = 0;
};
