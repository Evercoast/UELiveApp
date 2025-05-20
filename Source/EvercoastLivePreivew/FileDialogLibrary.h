// FileDialogLibrary.h
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FileDialogLibrary.generated.h"

UCLASS()
class UFileDialogLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "File Dialog")
    static FString OpenFileDialog();
};

