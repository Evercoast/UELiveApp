// Copied implementation from MediaSoundComponent.h
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SynthComponent.h"
#include "MediaSoundComponent.h"
#include "EvercoastMediaSoundComp.generated.h"

UCLASS(ClassGroup = Media, editinlinenew, meta = (BlueprintSpawnableComponent))
class EVERCOASTPLAYBACK_API UEvercoastMediaSoundComp
	: public UMediaSoundComponent
{
	GENERATED_BODY()

public:

	UEvercoastMediaSoundComp(const FObjectInitializer& ObjectInitializer);

	~UEvercoastMediaSoundComp();

public:
	void EnableAudioGeneration(bool enable);

protected:
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;
private:
	bool bAudioGenerationEnabled;
};
