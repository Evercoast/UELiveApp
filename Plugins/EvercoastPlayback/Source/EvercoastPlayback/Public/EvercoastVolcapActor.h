// Copyright (C) 2023 Evercoast Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EvercoastPerfCounter.h"
#include "EvercoastVolcapActor.generated.h"

class UEvercoastStreamingReaderComp;
class UEvercoastRendererSelectorComp;

UCLASS()
class EVERCOASTPLAYBACK_API AEvercoastVolcapActor : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleDefaultsOnly, Category = Misc)
	USceneComponent* DefaultRootComponent;
	
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Evercoast)
	UEvercoastStreamingReaderComp* Reader;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Evercoast)
	UEvercoastRendererSelectorComp* RendererSelector;

public:	
	// Sets default values for this actor's properties
	AEvercoastVolcapActor();
};
