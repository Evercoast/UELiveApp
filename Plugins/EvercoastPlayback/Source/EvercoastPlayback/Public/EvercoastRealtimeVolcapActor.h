// Copyright (C) 2023 Evercoast Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <memory>
#include "EvercoastPerfCounter.h"
#include "EvercoastRealtimeVolcapActor.generated.h"

class UPicoQuicStreamingReaderComp;
class UEvercoastRendererSelectorComp;
class UAudioComponent;
class EvercoastPerfCounter;

UCLASS()
class EVERCOASTPLAYBACK_API AEvercoastRealtimeVolcapActor : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleDefaultsOnly, Category = Misc)
	USceneComponent* DefaultRootComponent;

	std::unique_ptr<EvercoastPerfCounter> m_tickPerfCounter;

public:

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Livestreaming")
	void StartPreviewInEditor();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Livestreaming")
	void StopPreviewInEditor();
#endif

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Livestreaming")
	void Resync();


	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Profiling")
	float GetTickingRate();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Evercoast)
	UPicoQuicStreamingReaderComp* Reader;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Evercoast)
	UEvercoastRendererSelectorComp* RendererSelector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Evercoast)
	UAudioComponent* Audio;

public:
	AEvercoastRealtimeVolcapActor();
};
