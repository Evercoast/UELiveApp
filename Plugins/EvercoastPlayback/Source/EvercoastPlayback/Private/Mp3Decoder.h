#pragma once

#include "CoreMinimal.h"

struct RuntimeAudioMetadata;

class Mp3Decoder
{
public:
	static bool Validate(const uint8_t* data, int32_t dataSize);
	static bool Decode(const uint8_t* data, int32_t dataSize, RuntimeAudioMetadata* outMetadata, TArray<float>* outPCMData);
};