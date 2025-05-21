#include "LivePreviewPlayerController.h"
#include "LivePreviewPlayerCameraManager.h"
#include "ArcballPawn.h"
#include "RepositionFixedPawn.h"

ALivePreviewPlayerController::ALivePreviewPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Force maintain the Y FOV across different orientations and aspect ratios
	ULocalPlayer* DefaultLocalPlayer = ULocalPlayer::StaticClass()->GetDefaultObject<ULocalPlayer>();
	DefaultLocalPlayer->AspectRatioAxisConstraint = AspectRatio_MaintainYFOV;
	// override player camera initiation
	PlayerCameraManagerClass = ALivePreviewPlayerCameraManager::StaticClass();
	bShouldShowMouseCursor = true;
	CameraWalkingInterpDuration = 1.0f;

	InitialCameraOffsetZ = GConfig->GetFloatOrDefault(TEXT("Camera"), TEXT("InitialCameraZ"), 75.0f, GGameIni);
	InitialCameraDistance = GConfig->GetFloatOrDefault(TEXT("Camera"), TEXT("InitialCameraDistance"), 145.0f, GGameIni);
	InitialCameraRotation = FRotator::ZeroRotator;
	CameraOrbitSpeed = 0.15f;
	CameraWheelZoomSpeed = 5.0f;
	CameraOffsetZSpeed = 5.0f;
	CameraSweepingRadius = 15.0f;
	SavedCameraDistance = InitialCameraDistance;

	CameraXOffsetLimit = FVector2D(-100.0f, 100.0f);
	CameraZOffsetLimit = FVector2D(-100.0f, 200.0f);
	CameraPitchLimit = FVector2D(-80.0f, 15.0f);
	CameraZoomLimit = FVector2D(10.0f, 1000.0f);
	CameraPositionNegativeZLimit = -5.0f;
}

void ALivePreviewPlayerController::SetCameraWalkingInterpDuration(float duration)
{
	CameraWalkingInterpDuration = duration;

	auto* thePlayerCameraManager = Cast<ALivePreviewPlayerCameraManager>(PlayerCameraManager);
	thePlayerCameraManager->LocationBlendDuration = CameraWalkingInterpDuration;
}

void ALivePreviewPlayerController::SetInitialCameraOffsetZ(float offset)
{
	InitialCameraOffsetZ = offset;

	auto* thePlayerCameraManager = Cast<ALivePreviewPlayerCameraManager>(PlayerCameraManager);
	thePlayerCameraManager->FreeCamOffset = FVector(0, 0, offset);
}

void ALivePreviewPlayerController::SetInitialCameraDistance(float dist)
{
	InitialCameraDistance = dist;

	auto* thePlayerCameraManager = Cast<ALivePreviewPlayerCameraManager>(PlayerCameraManager);
	thePlayerCameraManager->FreeCamDistance = dist;
}

void ALivePreviewPlayerController::SetInitialCameraRotation(const FRotator& rotation)
{
	InitialCameraRotation = rotation;
}

void ALivePreviewPlayerController::SetCameraSweepingRadius(float radius)
{
	CameraSweepingRadius = radius;

	auto* thePlayerCameraManager = Cast<ALivePreviewPlayerCameraManager>(PlayerCameraManager);
	thePlayerCameraManager->SweepingCameraRadius = radius;
}

void ALivePreviewPlayerController::SetCameraPositionNegativeZLimit(float limit)
{
	CameraPositionNegativeZLimit = limit;

	auto* thePlayerCameraManager = Cast<ALivePreviewPlayerCameraManager>(PlayerCameraManager);
	thePlayerCameraManager->CameraPositionLimitNegativeZ = limit;
}

void ALivePreviewPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShouldShowMouseCursor = true;

	// override initial orbit orientation
	InitialActorRotation = GetControlRotation();
	FRotator initialRotator = InitialActorRotation + InitialCameraRotation;
	SetControlRotation(initialRotator);
}

void ALivePreviewPlayerController::HideMouseCursor()
{
	bShouldShowMouseCursor = false;
}

void ALivePreviewPlayerController::ShowMouseCursor()
{
	bShouldShowMouseCursor = true;
}

bool ALivePreviewPlayerController::ShouldShowMouseCursor() const
{
	return bShouldShowMouseCursor;
}

void ALivePreviewPlayerController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

void ALivePreviewPlayerController::SpawnPlayerCameraManager()
{
	Super::SpawnPlayerCameraManager();

	SetCameraMode(TEXT("Arcball"));

	CameraResetToInitial();
}

void ALivePreviewPlayerController::CameraOrbitByDelta(const FVector& Delta)
{
	FRotator OrbitRotation = GetControlRotation();
	OrbitRotation.Yaw += Delta.X * CameraOrbitSpeed;
	OrbitRotation.Pitch += Delta.Y * CameraOrbitSpeed;

	OrbitRotation.Pitch = FMath::Clamp(OrbitRotation.Pitch, CameraPitchLimit.X, CameraPitchLimit.Y);
	SetControlRotation(OrbitRotation);
}

void ALivePreviewPlayerController::CameraZoomByDelta(float Delta)
{
	check(PlayerCameraManager);
	float freeCamDist = PlayerCameraManager->FreeCamDistance;
	freeCamDist += Delta * CameraWheelZoomSpeed;

	freeCamDist = FMath::Clamp(freeCamDist, CameraZoomLimit.X, CameraZoomLimit.Y);

	PlayerCameraManager->FreeCamDistance = freeCamDist;
}

void ALivePreviewPlayerController::CameraZoomSaveCurrent()
{
	check(PlayerCameraManager);
	SavedCameraDistance = PlayerCameraManager->FreeCamDistance;

//	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, *FString::Printf(TEXT("Saved Cam Dist: %.3f"), SavedCameraDistance));
}

void ALivePreviewPlayerController::CameraZoomByPercentage(float Pct)
{
	check(PlayerCameraManager);

	
	float freeCamDist = FMath::Clamp(SavedCameraDistance * Pct, 10.0f, 1000.0f);
//	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, *FString::Printf(TEXT("New Cam Dist: %.3f"), freeCamDist));

	PlayerCameraManager->FreeCamDistance = freeCamDist;
}

void ALivePreviewPlayerController::CameraTargetOffsetByDelta(float DeltaX, float DeltaY, bool ApplySpeed)
{
	check(PlayerCameraManager);
	FVector freeCamOffset = PlayerCameraManager->FreeCamOffset;
	if (ApplySpeed)
	{
		freeCamOffset.Y += DeltaX * CameraOffsetZSpeed;
		freeCamOffset.Z += DeltaY * CameraOffsetZSpeed;
	}
	else
	{
		freeCamOffset.Y += DeltaX;
		freeCamOffset.Z += DeltaY;
	}

	freeCamOffset.Y = FMath::Clamp(freeCamOffset.Y, CameraXOffsetLimit.X, CameraXOffsetLimit.Y);
	freeCamOffset.Z = FMath::Clamp(freeCamOffset.Z, CameraZOffsetLimit.X, CameraZOffsetLimit.Y);


	PlayerCameraManager->FreeCamOffset = freeCamOffset;
}


bool ALivePreviewPlayerController::MoveToByDeprojectingScreenPosition(float screenX, float screenY)
{
	ALivePreviewPlayerCameraManager* thePlayerCameraManager = Cast<ALivePreviewPlayerCameraManager>(PlayerCameraManager);
	check(thePlayerCameraManager);

	FVector2D MouseLoc(screenX, screenY);
	FCollisionQueryParams Params;
	APawn* ThePawn = GetPawn();
	//Params.AddIgnoredActor(ThePawn);
	Params.AddIgnoredActor(thePlayerCameraManager);
	FHitResult HitResult;
	// de-project screen position to world position
	if (GetHitResultAtScreenPosition(MouseLoc, ECollisionChannel::ECC_WorldStatic, Params, HitResult))
	{
		if (WalkingSurfaceActors.Num() == 0)
		{
			if (FVector::DotProduct(HitResult.Normal, FVector::UpVector) >= 0.95f)
			{
				thePlayerCameraManager->SaveCurrentCameraPOV();
				ThePawn->TeleportTo(HitResult.Location, ThePawn->GetActorRotation(), false, false);
				return true;
			}
		}
		else
		{
#if ENGINE_MAJOR_VERSION == 5
			if (WalkingSurfaceActors.Contains(HitResult.GetActor()))
#else
			if (WalkingSurfaceActors.Contains(HitResult.Actor))
#endif
			{
				thePlayerCameraManager->SaveCurrentCameraPOV();
				ThePawn->TeleportTo(HitResult.Location, ThePawn->GetActorRotation(), false, false);
				return true;
			}
		}
	}

	return false;
}

void ALivePreviewPlayerController::CameraResetToInitial()
{
	check(PlayerCameraManager);
	auto* thePlayerCameraManager = Cast<ALivePreviewPlayerCameraManager>(PlayerCameraManager);

	// walking interop duration
	thePlayerCameraManager->LocationBlendDuration = CameraWalkingInterpDuration;
	// Z offset
	thePlayerCameraManager->FreeCamOffset = FVector(0, 0, InitialCameraOffsetZ);
	// orbit distance
	thePlayerCameraManager->FreeCamDistance = InitialCameraDistance;
	// Sweep size
	thePlayerCameraManager->SweepingCameraRadius = CameraSweepingRadius;
	// Z downwards limit
	thePlayerCameraManager->CameraPositionLimitNegativeZ = CameraPositionNegativeZLimit;

	thePlayerCameraManager->SaveCurrentCameraPOV();

	FRotator initialRotator = InitialActorRotation + InitialCameraRotation;
	SetControlRotation(initialRotator);
}

void ALivePreviewPlayerController::SwitchToFirstPersonCameraMode()
{
	APawn* currentPawn = GetPawn();
	APawn* newPawn = OnSpawnFirstPersonPawn();
	if (!newPawn)
	{
		FActorSpawnParameters params;
		params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		params.TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot;
		params.Owner = this;
		params.Instigator = GetInstigator();
		newPawn = GetWorld()->SpawnActor<ADefaultPawn>(params);

		
	}

	if (currentPawn)
	{
		newPawn->SetActorTransform(currentPawn->GetActorTransform());
	}

	auto* thePlayerCameraManager = Cast<ALivePreviewPlayerCameraManager>(PlayerCameraManager);
	thePlayerCameraManager->UpdatePawnReference(newPawn);

	Possess(newPawn);

	if (currentPawn)
	{
		currentPawn->Destroy();
	}

	SetCameraMode(TEXT("FirstPerson"));
	
	
}

void ALivePreviewPlayerController::SwitchToArcballCameraMode(float autoRotateSpeed)
{
	APawn* currentPawn = GetPawn();
	
	APawn* newPawn = OnSpawnArcballPawn();
	if (!newPawn)
	{
		FActorSpawnParameters params;
		params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		params.TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot;
		params.Owner = this;
		params.Instigator = GetInstigator();
		newPawn = GetWorld()->SpawnActor<AArcballPawn>(params);
	}

	AArcballPawn* arcball = static_cast<AArcballPawn*>(newPawn);
	arcball->AutoRotateSpeed = autoRotateSpeed;

	auto* thePlayerCameraManager = Cast<ALivePreviewPlayerCameraManager>(PlayerCameraManager);
	if (currentPawn)
	{
		InitialActorRotation = GetControlRotation();
		FRotator initialRotator = InitialActorRotation + InitialCameraRotation;

		SetControlRotation(initialRotator);

		// ArcballPawn will be set to the ActorballCentre actor's place with offset
		if (ArcballCenter)
		{
			newPawn->SetActorLocation(ArcballCenter->GetActorLocation() + FVector(0, 0, InitialCameraOffsetZ));
		}
	}

	thePlayerCameraManager->UpdatePawnReference(newPawn);

	Possess(newPawn);

	if (currentPawn)
	{
		currentPawn->Destroy();
	}

	SetCameraMode(TEXT("Arcball"));
}


void ALivePreviewPlayerController::SwitchToFixedCameraMode(AActor* RepositionTarget)
{
	APawn* currentPawn = GetPawn();

	APawn* newPawn = OnSpawnRepositionFixedPawn(RepositionTarget);
	if (!newPawn)
	{
		FActorSpawnParameters params;
		params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		params.TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot;
		params.Owner = this;
		params.Instigator = GetInstigator();
		newPawn = GetWorld()->SpawnActor<ARepositionFixedPawn>(params);

		
	}

	if (currentPawn)
	{
		newPawn->SetActorTransform(currentPawn->GetActorTransform());
	}

	auto* thePlayerCameraManager = Cast<ALivePreviewPlayerCameraManager>(PlayerCameraManager);
	thePlayerCameraManager->UpdatePawnReference(newPawn);

	Possess(newPawn);

	if (currentPawn)
	{
		currentPawn->Destroy();
	}

	SetCameraMode(TEXT("Fixed"));
}

void ALivePreviewPlayerController::OnPossess(APawn* pawn)
{
	Super::OnPossess(pawn);

	if (AArcballPawn* ArcBallPawn = Cast<AArcballPawn>(pawn))
	{
		SetCameraMode(TEXT("Arcball"));
	}
	else if (ARepositionFixedPawn* FixedPawn = Cast<ARepositionFixedPawn>(pawn))
	{
		SetCameraMode(TEXT("Fixed"));
	}
	else
	{
		SetCameraMode(TEXT("FirstPerson"));
	}
}

void ALivePreviewPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);
}

void ALivePreviewPlayerController::UniformScaleTargetByMouseDelta(AActor* ScaleTarget, float Delta)
{

	if (ScaleTarget)
	{
		FTransform newTransform = ScaleTarget->GetActorTransform();
		FVector currScale = newTransform.GetScale3D();
		float newScaleScalar = currScale.X + Delta;
		
		if (newScaleScalar < 0.5f)
		{
			newScaleScalar = 0.5f;
		}
		if (newScaleScalar > 1.5f)
		{
			newScaleScalar = 1.5f;
		}
		
		newTransform.SetScale3D(FVector(newScaleScalar, newScaleScalar, newScaleScalar));

		ScaleTarget->SetActorTransform(newTransform);
	}
}

void ALivePreviewPlayerController::RotateTargetByMouseDelta(AActor* RotateTarget, float mouseZ)
{
	if (RotateTarget)
	{
		FTransform newTransform = RotateTarget->GetActorTransform();
		FQuat newRotation = newTransform.GetRotation();
		newRotation = newRotation * FQuat::MakeFromEuler(FVector(0, 0, mouseZ));
		newTransform.SetRotation(newRotation);
		RotateTarget->SetActorTransform(newTransform);
	}
}


bool ALivePreviewPlayerController::MoveTargetByDeprojectingScreenPosition(AActor* MoveTarget, float screenX, float screenY)
{
	ALivePreviewPlayerCameraManager* thePlayerCameraManager = Cast<ALivePreviewPlayerCameraManager>(PlayerCameraManager);
	if (!thePlayerCameraManager || !MoveTarget)
		return false;

	FVector2D MouseLoc(screenX, screenY);
	FCollisionQueryParams Params;
	APawn* ThePawn = GetPawn();
	Params.AddIgnoredActor(ThePawn);
	Params.AddIgnoredActor(thePlayerCameraManager);
	FHitResult HitResult;
	// de-project screen position to world position
	if (GetHitResultAtScreenPosition(MouseLoc, ECollisionChannel::ECC_WorldStatic, Params, HitResult))
	{
		if (WalkingSurfaceActors.Num() == 0)
		{
			if (FVector::DotProduct(HitResult.Normal, FVector::UpVector) >= 0.95f)
			{
				thePlayerCameraManager->SaveCurrentCameraPOV();
				
				FTransform newTransform = MoveTarget->GetActorTransform();
				newTransform.SetLocation(HitResult.Location);
				MoveTarget->SetActorTransform(newTransform);
				return true;
			}
		}
		else
		{
#if ENGINE_MAJOR_VERSION == 5
			if (WalkingSurfaceActors.Contains(HitResult.GetActor()))
#else
			if (WalkingSurfaceActors.Contains(HitResult.Actor))
#endif
			{
				thePlayerCameraManager->SaveCurrentCameraPOV();

				FTransform newTransform = MoveTarget->GetActorTransform();
				newTransform.SetLocation(HitResult.Location);
				MoveTarget->SetActorTransform(newTransform);
				return true;
			}
		}
	}

	return false;
}