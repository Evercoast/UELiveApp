#include "Gaussian/GaussianSplatCompositeSubsystem.h"
#include "Gaussian/GaussianSplatOffscreenRendererSceneViewExtension.h"
#include "SceneViewExtension.h"

void UGaussianSplatCompositeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CompositeSceneViewExtension = FSceneViewExtensions::NewExtension<FGaussianSplatOffscreenRendererSceneViewExtension>();
	CompositeSceneViewExtension->Initialize();
}

void UGaussianSplatCompositeSubsystem::Deinitialize()
{
	if (CompositeSceneViewExtension)
	{
		CompositeSceneViewExtension->IsActiveThisFrameFunctions.Empty();

		FSceneViewExtensionIsActiveFunctor IsActiveFunctor;

		IsActiveFunctor.IsActiveFunction = [](const ISceneViewExtension* SceneViewExtension, const FSceneViewExtensionContext& Context)
			{
				return TOptional<bool>(false);
			};

		CompositeSceneViewExtension->IsActiveThisFrameFunctions.Add(IsActiveFunctor);

		CompositeSceneViewExtension->Deinitialize();
	}

	CompositeSceneViewExtension.Reset();
	CompositeSceneViewExtension = nullptr;

	Super::Deinitialize();
}
