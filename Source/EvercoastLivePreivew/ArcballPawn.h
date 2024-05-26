#pragma once
#include "CoreMinimal.h"
#include "GameFramework/DefaultPawn.h"
#include <map>
#include "ArcballPawn.generated.h"


class UStaticMeshComponent;
class UInputComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class AArcballPawn : public ADefaultPawn // from DefaultPawn/ACharacter?
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadonly, VisibleAnywhere, Category = Mesh)
	UStaticMeshComponent* MeshIndicator;

	AArcballPawn(const FObjectInitializer& Initializer);
	virtual void Tick(float DeltaSeconds) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Misc)
	float IndicatorHideTimer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Misc)
	bool IgnoreInput;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Misc)
	bool DisableMoving;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Misc)
	float AutoRotateSpeed;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	void OnMouseLeftButtonPressed();
	void OnMouseLeftButtonReleased();

	void OnTouchBegan(ETouchIndex::Type, FVector);
	void OnTouchEnded(ETouchIndex::Type, FVector);
	void OnTouchMoved(ETouchIndex::Type, FVector);

	void OnMouseMoving(FVector Val);
	void OnMouseWheel(float Val);
	void CameraZoomByPinch(float Pinch);
	void CameraTargetOffset(float X, float Z);

	void OnMouseRightButtonPressed();
	void OnMouseRightButtonReleased();

protected:
	bool OffsetZModifier;
	bool OrbitModifier;
	FVector2D LastMousePressLocation;
	float LastMousePressTime;

	FVector LastTouchBeginLocation[2];
	FVector LastTouchMoveLocation[2];
	float LastTouchBeginTime[2];

	std::map<ETouchIndex::Type, int> TouchRegistry;
	

	float IndicatorHideTimeToGo;
	int TouchCount;
};