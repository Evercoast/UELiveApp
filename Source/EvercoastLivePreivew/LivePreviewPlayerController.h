#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LivePreviewPlayerController.generated.h"


class APawn;
class ADefaultPawn;
class AArcballPawn;
class ARepositionFixedPawn;

UCLASS()
class ALivePreviewPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	/** Default Constructor */
	ALivePreviewPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostInitializeComponents() override;
	virtual void SpawnPlayerCameraManager() override;

	virtual bool ShouldShowMouseCursor() const override;

	void ShowMouseCursor();
	void HideMouseCursor();

	virtual void SetPawn(APawn* InPawn) override;

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

private:
	bool bShouldShowMouseCursor;
	float SavedCameraDistance;

	FRotator InitialActorRotation;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter=SetCameraWalkingInterpDuration, Category = Camera)
	float CameraWalkingInterpDuration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetInitialCameraOffsetZ, Category = Camera)
	float InitialCameraOffsetZ;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetInitialCameraDistance, Category = Camera)
	float InitialCameraDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetInitialCameraRotation, Category = Camera)
	FRotator InitialCameraRotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	float CameraOrbitSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	float CameraWheelZoomSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	float CameraOffsetZSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetCameraSweepingRadius, Category = Camera)
	float CameraSweepingRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
	TArray<AActor*> WalkingSurfaceActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	FVector2D CameraXOffsetLimit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	FVector2D CameraZOffsetLimit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	FVector2D CameraPitchLimit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetCameraPositionNegativeZLimit, Category = Camera)
	float CameraPositionNegativeZLimit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	FVector2D CameraZoomLimit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
	AActor* LivestreamRoot;

	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetCameraWalkingInterpDuration(float duration);

	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetInitialCameraOffsetZ(float offset);

	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetInitialCameraDistance(float distance);

	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetInitialCameraRotation(const FRotator& rotation);

	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetCameraSweepingRadius(float radius);

	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetCameraPositionNegativeZLimit(float limit);

	UFUNCTION(BlueprintCallable, Category = Camera)
	void CameraOrbitByDelta(const FVector& Delta);

	UFUNCTION(BlueprintCallable, Category = Camera)
	void CameraZoomByDelta(float Delta);

	UFUNCTION(BlueprintCallable, Category = Camera)
	void CameraZoomSaveCurrent();

	UFUNCTION(BlueprintCallable, Category = Camera)
	void CameraZoomByPercentage(float Pct);

	UFUNCTION(BlueprintCallable, Category = Camera)
	void CameraTargetOffsetByDelta(float DeltaX, float DeltaY, bool applySpeed);

	UFUNCTION(BlueprintCallable, Category = Movement)
	bool MoveToByDeprojectingScreenPosition(float screenX, float screenY);

	UFUNCTION(BlueprintCallable, Category = Camera)
	void CameraResetToInitial();

	UFUNCTION(BlueprintCallable, Category = Gameplay)
	void SwitchToFirstPersonCameraMode();

	UFUNCTION(BlueprintCallable, Category = Gameplay)
	void SwitchToArcballCameraMode(float autoRotateSpeed);

	UFUNCTION(BlueprintCallable, Category = Gameplay)
	void SwitchToFixedCameraMode();

	UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly, meta = (DisplayName = "SpawnArcballPawn"))
	AArcballPawn* OnSpawnArcballPawn();

	UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly, meta = (DisplayName = "SpawnFirstPersonPawn"))
	ADefaultPawn* OnSpawnFirstPersonPawn();

	UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly, meta = (DisplayName = "SpawnRepositionFixedPawn"))
	ARepositionFixedPawn* OnSpawnRepositionFixedPawn();

	UFUNCTION(BlueprintCallable, Category = Gameplay)
	void RotateLivestreamRootByMouseDelta(float mouseX);

	UFUNCTION(BlueprintCallable, Category = Gameplay)
	bool MoveLivestreamRootByDeprojectingScreenPosition(float screenX, float screenY);
};