#pragma once
#include "CoreMinimal.h"
#include "LivePreviewPlayerCameraManager.generated.h"

class ALivePreviewPlayerController;
class UCapsuleComponent;
class APawn;
UCLASS()
class ALivePreviewPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

	friend class ALivePreviewPlayerController;
public:

	ALivePreviewPlayerCameraManager(const FObjectInitializer& Initializer);

	void SaveCurrentCameraPOV();

	void UpdatePawnReference(APawn* pawn);

	UFUNCTION(BlueprintCallable)
	bool IsLocationBlendFinished() const;

	UPROPERTY(EditAnywhere, BlueprintReadonly, Category = Collision)
	UCapsuleComponent* CapsuleComponent;

	

protected:
	virtual void UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime) override;

private:
	FMinimalViewInfo SavedCameraPOV;
	FMinimalViewInfo CurrCameraPOV;
	FVector SavedFreeCamOffset;
	float SavedFreeCamDist;

	struct FViewTargetTransitionParams LocationBlendParams;
	float LocationBlendTimeToGo;

	float LocationBlendDuration;
	FVector InitialCameraOffset;

	float SweepingCameraRadius;

	float CameraPositionLimitNegativeZ;

	APawn* IgnoredPawn;
	
};