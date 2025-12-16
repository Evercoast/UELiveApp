#pragma once


#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "GaussianSplatCompositeSubsystem.generated.h"

class FGaussianSplatOffscreenRendererSceneViewExtension;
UCLASS()
class UGaussianSplatCompositeSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	TSharedPtr<class FGaussianSplatOffscreenRendererSceneViewExtension, ESPMode::ThreadSafe> CompositeSceneViewExtension;

};