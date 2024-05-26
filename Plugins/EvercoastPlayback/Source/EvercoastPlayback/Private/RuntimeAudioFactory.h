#pragma once

#include "CoreMinimal.h"
#include <memory>
#include "Sound/SoundWave.h"

class FRunnableThread;
class URuntimeAudio;
enum class ERuntimeAudioFactoryResult
{
	Succeeded = 0,
	Failed_Reading,
	Failed_Decoding,
	Failed_CreatingAudioObject,
	Failed_Unknown
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRuntimeAudioFactoryResult, URuntimeAudio* audio, ERuntimeAudioFactoryResult result)

class FRuntimeAudioFactory
{
public:
	static URuntimeAudio* CreateRuntimeAudioFromBufferSync(const TArray<uint8>& buffer);
	static 
	// can't get this pass Android compiler tho c++17 was set
	std::shared_ptr<FRunnableThread> CreateRuntimeAudioFromBuffer(const TArray<uint8>& buffer, FOnRuntimeAudioFactoryResult callback);
};
