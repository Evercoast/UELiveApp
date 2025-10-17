#include "Gaussian/GaussianSplatCompositeSubsystem.h"
#include "Gaussian/GaussianSplatTileRendererSceneViewExtension.h"
#include "SceneViewExtension.h"

void UGaussianSplatCompositeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if PLATFORM_WINDOWS
	CompositeSceneViewExtension = FSceneViewExtensions::NewExtension<FGaussianSplatTileRendererSceneViewExtension>();
	CompositeSceneViewExtension->Initialize();
#endif
}

void UGaussianSplatCompositeSubsystem::Deinitialize()
{
#if PLATFORM_WINDOWS
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
#endif
	Super::Deinitialize();
}
