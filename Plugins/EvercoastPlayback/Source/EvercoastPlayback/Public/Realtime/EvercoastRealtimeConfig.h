#pragma once
#include "CoreMinimal.h"
#include "EvercoastRealtimeConfig.generated.h"

UCLASS(Config="EvercoastRealtime", Blueprintable, BlueprintType)
class EVERCOASTPLAYBACK_API UEvercoastRealtimeConfig : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, BlueprintReadOnly)
    bool UseOldPicoQuic;
    UPROPERTY(Config, BlueprintReadOnly)
    FString CertificationPath;
    UPROPERTY(Config, BlueprintReadOnly)
    FString AccessToken;
    UPROPERTY(Config, BlueprintReadOnly)
    float WarmUpTime = 0.0f;
};
