#pragma once

#include "CoreMinimal.h"

struct RuntimeAudioMetadata;

class WavDecoder
{
public:
	static bool ValidateSimple(const uint8_t* data, int32_t dataSize);
	static bool ValidateAndFixDuration(uint8_t* data, int32_t dataSize);
	static bool Decode(const uint8_t* data, int32_t dataSize, RuntimeAudioMetadata* outMetadata, TArray<float>* outPCMData);
};
