#pragma once

#include <memory>
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"
#include "NiagaraComponent.h"
#include "EvercoastGaussianSplatRendererComp.generated.h"


class EvercoastLocalSpzFrame;
class EvercoastGaussianSplatUploader;
class IEvercoastStreamingDataUploader;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class EVERCOASTPLAYBACK_API UEvercoastGaussianSplatRendererComp : public UNiagaraComponent //UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UEvercoastGaussianSplatRendererComp(const FObjectInitializer& ObjectInitializer);

	void SetGaussianSplatData(std::shared_ptr<EvercoastLocalSpzFrame> spzFrame);

	std::shared_ptr<IEvercoastStreamingDataUploader> GetDataUploader() const;

private:
	std::shared_ptr<EvercoastGaussianSplatUploader> m_gaussianUploader;
};