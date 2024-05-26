// Copyright (C) 2023 Evercoast Inc. All Rights Reserved.
#include "EvercoastRealtimeVolcapActor.h"
#include "PicoQuicStreamingReaderComp.h"
#include "EvercoastRendererSelectorComp.h"
#include "EvercoastPerfCounter.h"
#include "Components/AudioComponent.h"

// Sets default values
AEvercoastRealtimeVolcapActor::AEvercoastRealtimeVolcapActor()
{
	DefaultRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRootComponent"));

	RendererSelector = CreateDefaultSubobject<UEvercoastRendererSelectorComp>(TEXT("RendererSelector"));
	RendererSelector->SetupAttachment(DefaultRootComponent);

	Audio = CreateDefaultSubobject<UAudioComponent>(TEXT("Audio"));
	Audio->SetupAttachment(DefaultRootComponent);

	Reader = CreateDefaultSubobject<UPicoQuicStreamingReaderComp>(TEXT("Reader"));
	Reader->SetRendererActor(this);

	RootComponent = DefaultRootComponent;

	PrimaryActorTick.bCanEverTick = true;

	m_tickPerfCounter = std::make_unique<EvercoastPerfCounter>("Tick Rate", 2.0f);
}


void AEvercoastRealtimeVolcapActor::Tick(float DeltaTime)
{
	m_tickPerfCounter->AddSample();
}

float AEvercoastRealtimeVolcapActor::GetTickingRate()
{
	return (float)m_tickPerfCounter->GetSampleAverageInt64OnDuration();
}

#if WITH_EDITOR
void AEvercoastRealtimeVolcapActor::StartPreviewInEditor()
{
	Reader->StartPreviewInEditor();
}

void AEvercoastRealtimeVolcapActor::StopPreviewInEditor()
{
	Reader->StopPreviewInEditor();
}
#endif