#include "EvercoastMediaSoundComp.h"

/* UEvercoastMediaSoundComp structors
 *****************************************************************************/

UEvercoastMediaSoundComp::UEvercoastMediaSoundComp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bAudioGenerationEnabled(false)
{
}


UEvercoastMediaSoundComp::~UEvercoastMediaSoundComp()
{
}

void UEvercoastMediaSoundComp::EnableAudioGeneration(bool enable)
{
	bAudioGenerationEnabled = enable;
}


int32 UEvercoastMediaSoundComp::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	int32 ProcessedNumSamples = Super::OnGenerateAudio(OutAudio, NumSamples);

	if (!bAudioGenerationEnabled)
	{
		// re-clear the output buffer
		memset(OutAudio, 0, NumSamples * sizeof(float));
		ProcessedNumSamples = 0;
	}

	return ProcessedNumSamples;
}
