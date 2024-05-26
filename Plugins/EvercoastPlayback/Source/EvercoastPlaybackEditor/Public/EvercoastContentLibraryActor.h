// Copyright (C) 2021 Evercoast Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EvercoastContentLibraryActor.generated.h"

UCLASS()
class EVERCOASTPLAYBACKEDITOR_API AEvercoastContentLibraryActor : public AActor
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleDefaultsOnly, Category = Misc)
	USceneComponent* DefaultRootComponent;

	UPROPERTY()
	class UBillboardComponent* EditorSprite;
public:	
	// Sets default values for this actor's properties
	AEvercoastContentLibraryActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void PostLoad() override;

public:	

	UFUNCTION(BlueprintCallable, CallInEditor, Category=Evercoast)
	void OpenEvercoastLibrary();
private:
	void OnSelectionChanged(UObject* Object);

	bool bHasOpenedURL;
	bool bHasLoadedFromLevel;
};
