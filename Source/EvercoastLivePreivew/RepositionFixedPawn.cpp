#include "RepositionFixedPawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LivePreviewPlayerController.h"
#include "LivePreviewPlayerCameraManager.h"
#include "GameFramework/InputSettings.h"
#include "Math/UnrealMathUtility.h"


ARepositionFixedPawn::ARepositionFixedPawn(const FObjectInitializer& Initializer)  :
	Super(Initializer)
{
	// Set size for collision capsule
	GetCollisionComponent()->InitSphereRadius(30.f);
	GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// No default mesh
	if (auto* meshComp = GetMeshComponent())
	{
		meshComp->DestroyComponent();
		// no way to set the actual member to null, just don't call GetMeshComponent() anymore
	}

	RepositionXYModifier = false;
	RotationZModifier = false;
	IgnoreInput = false;
	DisableMoving = true;

	bAddDefaultMovementBindings = false;
}

void ARepositionFixedPawn::BeginPlay()
{
	Super::BeginPlay();

	RepositionXYModifier = false;
	RotationZModifier = false;
}

void ARepositionFixedPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);

	// Bind point and go events
	PlayerInputComponent->BindKey(FInputChord(EKeys::RightMouseButton), IE_Pressed, this, &ARepositionFixedPawn::OnMouseRightButtonPressed);
	PlayerInputComponent->BindKey(FInputChord(EKeys::RightMouseButton), IE_Released, this, &ARepositionFixedPawn::OnMouseRightButtonReleased);

	PlayerInputComponent->BindKey(FInputChord(EKeys::LeftMouseButton), IE_Pressed, this, &ARepositionFixedPawn::OnMouseLeftButtonPressed);
	PlayerInputComponent->BindKey(FInputChord(EKeys::LeftMouseButton), IE_Released, this, &ARepositionFixedPawn::OnMouseLeftButtonReleased);

	PlayerInputComponent->BindVectorAxis(EKeys::Mouse2D, this, &ARepositionFixedPawn::OnMouseMoving);

	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ARepositionFixedPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}


void ARepositionFixedPawn::OnMouseMoving(FVector Value)
{
	if (IgnoreInput)
		return;

	if (Value.SizeSquared() > 0.0f)
	{
		if (ALivePreviewPlayerController* PlayerController = Cast<ALivePreviewPlayerController>(Controller))
		{
			if (RepositionXYModifier && RotationZModifier)
			{
				// Scale
				UniformScaleFromMouseDelta(Value.Y / PlayerController->PlayerInput->GetMouseSensitivityY() * 0.01f);
			}
			if (RepositionXYModifier)
			{
				ExtrapolatedMouseLocation.X += Value.X;
				ExtrapolatedMouseLocation.Y -= Value.Y;
				RepositionXYFromMousePosition(ExtrapolatedMouseLocation);
			}
			else
			if (RotationZModifier)
			{
				RotateZFromMouseDelta(Value.X / PlayerController->PlayerInput->GetMouseSensitivityX(), 
					Value.Y / PlayerController->PlayerInput->GetMouseSensitivityY());
			}
		}
	}
}

void ARepositionFixedPawn::UniformScaleFromMouseDelta(float Delta)
{
	if (IgnoreInput)
		return;

	if (Delta != 0.0f)
	{
		if (ALivePreviewPlayerController* PlayerController = Cast<ALivePreviewPlayerController>(Controller))
		{
			check(PlayerController->PlayerCameraManager);

			PlayerController->UniformScaleTargetByMouseDelta(RepositionTarget, Delta);
		}
	}
}

void ARepositionFixedPawn::RotateZFromMouseDelta(float X, float Z)
{
	if (IgnoreInput)
		return;

	if (X != 0.0f || Z != 0.0f)
	{
		if (ALivePreviewPlayerController* PlayerController = Cast<ALivePreviewPlayerController>(Controller))
		{
			check(PlayerController->PlayerCameraManager);

			PlayerController->RotateTargetByMouseDelta(RepositionTarget, X);
		}
	}
}

void ARepositionFixedPawn::OnMouseRightButtonPressed()
{
	RotationZModifier = true;
}

void ARepositionFixedPawn::OnMouseRightButtonReleased()
{
	RotationZModifier = false;
}

void ARepositionFixedPawn::OnMouseLeftButtonPressed()
{
	if (IgnoreInput)
		return;

	if (ALivePreviewPlayerController* PlayerController = Cast<ALivePreviewPlayerController>(Controller))
	{
		float mouseX, mouseY;
		if (PlayerController->GetMousePosition(mouseX, mouseY))
		{
			FVector2D MouseLoc = FVector2D(mouseX, mouseY);
			LastMousePressLocation = MouseLoc;
			ExtrapolatedMouseLocation = MouseLoc;
			RepositionXYFromMousePosition(MouseLoc);
			RepositionXYModifier = true;
		}
	}
}

void ARepositionFixedPawn::RepositionXYFromMousePosition(const FVector2D& MouseLoc)
{
	if (ALivePreviewPlayerController* PlayerController = Cast<ALivePreviewPlayerController>(Controller))
	{
		if (PlayerController->MoveTargetByDeprojectingScreenPosition(RepositionTarget, MouseLoc.X, MouseLoc.Y))
		{
			// success
		}
	}
}

void ARepositionFixedPawn::OnMouseLeftButtonReleased()
{
	if (IgnoreInput)
		return;

	if (ALivePreviewPlayerController* PlayerController = Cast<ALivePreviewPlayerController>(Controller))
	{
		if (RepositionXYModifier)
		{
			RepositionXYFromMousePosition(ExtrapolatedMouseLocation);

			PlayerController->SetMouseLocation((int)ExtrapolatedMouseLocation.X, (int)ExtrapolatedMouseLocation.Y);

			RepositionXYModifier = false;
		}
	}
}

