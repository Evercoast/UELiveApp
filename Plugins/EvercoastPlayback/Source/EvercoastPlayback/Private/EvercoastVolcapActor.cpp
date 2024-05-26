// Copyright (C) 2023 Evercoast Inc. All Rights Reserved.


#include "EvercoastVolcapActor.h"
#include "EvercoastStreamingReaderComp.h"
#include "EvercoastRendererSelectorComp.h"

// Sets default values
AEvercoastVolcapActor::AEvercoastVolcapActor()
{
	DefaultRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRootComponent"));
	
	RendererSelector = CreateDefaultSubobject<UEvercoastRendererSelectorComp>(TEXT("RendererSelector"));
	RendererSelector->SetupAttachment(DefaultRootComponent);

	Reader = CreateDefaultSubobject<UEvercoastStreamingReaderComp>(TEXT("Reader"));
	Reader->SetRendererActor(this);

	RootComponent = DefaultRootComponent;

	PrimaryActorTick.bCanEverTick = true;
}
