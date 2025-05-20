#pragma once
#include "CoreMinimal.h"
#include "GameFramework/DefaultPawn.h"
#include "RepositionFixedPawn.generated.h"


class UStaticMeshComponent;
class UInputComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class ARepositionFixedPawn : public ADefaultPawn
{
	GENERATED_BODY()

public:
	ARepositionFixedPawn(const FObjectInitializer& Initializer);
	virtual void Tick(float DeltaSeconds) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Misc)
	bool IgnoreInput;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Misc)
	bool DisableMoving;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Misc)
	AActor* RepositionTarget;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	void OnMouseLeftButtonPressed();
	void OnMouseLeftButtonReleased();

	void OnMouseMoving(FVector Val);

	void OnMouseRightButtonPressed();
	void OnMouseRightButtonReleased();

protected:
	bool RepositionXYModifier;
	bool RotationZModifier;
	FVector2D LastMousePressLocation;
	FVector2D ExtrapolatedMouseLocation;

private:
	void RotateZFromMouseDelta(float X, float Z);
	void RepositionXYFromMousePosition(const FVector2D& MouseLoc);
};