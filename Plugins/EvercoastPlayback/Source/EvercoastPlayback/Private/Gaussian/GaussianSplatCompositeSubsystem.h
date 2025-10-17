#pragma once


#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "GaussianSplatCompositeSubsystem.generated.h"

class FGaussianSplatTileRendererSceneViewExtension;
UCLASS()
class UGaussianSplatCompositeSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
#if PLATFORM_WINDOWS
	TSharedPtr<class FGaussianSplatTileRendererSceneViewExtension, ESPMode::ThreadSafe> CompositeSceneViewExtension;
#endif
};