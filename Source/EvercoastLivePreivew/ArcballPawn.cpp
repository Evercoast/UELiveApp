#include "ArcballPawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LivePreviewPlayerController.h"
#include "LivePreviewPlayerCameraManager.h"
#include "GameFramework/InputSettings.h"
#include "Math/UnrealMathUtility.h"


AArcballPawn::AArcballPawn(const FObjectInitializer& Initializer)  :
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

	MeshIndicator = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshIndicator1"));
	MeshIndicator->SetOnlyOwnerSee(false);
	MeshIndicator->SetupAttachment(GetCollisionComponent());
	MeshIndicator->bCastDynamicShadow = false;
	MeshIndicator->CastShadow = false;
	MeshIndicator->SetRelativeRotation(FRotator(0, 0, 0));
	MeshIndicator->SetRelativeLocation(FVector(0, 0, 0));
	// Do not interfere with ray casting
	MeshIndicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	OffsetZModifier = false;
	LastMousePressTime = 0;
	OrbitModifier = false;
	IndicatorHideTimer = 1.0f;
	IndicatorHideTimeToGo = 0;
	TouchCount = 0;
	IgnoreInput = false;
	DisableMoving = true;
	AutoRotateSpeed = 0.f;

	bAddDefaultMovementBindings = false;
}

void AArcballPawn::BeginPlay()
{
	Super::BeginPlay();

	MeshIndicator->SetHiddenInGame(true, true);

	OffsetZModifier = false;
	LastMousePressTime = 0;
	OrbitModifier = false;
	IndicatorHideTimer = 1.0f;
	IndicatorHideTimeToGo = 0;
	TouchCount = 0;
	TouchRegistry.clear();

}

void AArcballPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);

	// Bind point and go events
	PlayerInputComponent->BindKey(FInputChord(EKeys::RightMouseButton), IE_Pressed, this, &AArcballPawn::OnMouseRightButtonPressed);
	PlayerInputComponent->BindKey(FInputChord(EKeys::RightMouseButton), IE_Released, this, &AArcballPawn::OnMouseRightButtonReleased);

	PlayerInputComponent->BindKey(FInputChord(EKeys::LeftMouseButton), IE_Pressed, this, &AArcballPawn::OnMouseLeftButtonPressed);
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AArcballPawn::OnTouchBegan);

	PlayerInputComponent->BindKey(FInputChord(EKeys::LeftMouseButton), IE_Released, this, &AArcballPawn::OnMouseLeftButtonReleased);
	PlayerInputComponent->BindTouch(IE_Released, this, &AArcballPawn::OnTouchEnded);

	PlayerInputComponent->BindVectorAxis(EKeys::Mouse2D, this, &AArcballPawn::OnMouseMoving);
	PlayerInputComponent->BindTouch(IE_Repeat, this, &AArcballPawn::OnTouchMoved);

	PlayerInputComponent->BindAxisKey(EKeys::MouseWheelAxis, this, &AArcballPawn::OnMouseWheel);

	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AArcballPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IndicatorHideTimeToGo > 0)
	{
		IndicatorHideTimeToGo -= DeltaSeconds;

		if (IndicatorHideTimeToGo <= 0)
		{
			MeshIndicator->SetHiddenInGame(true, true);
		}
	}

	if (AutoRotateSpeed != 0)
	{
		if (ALivePreviewPlayerController* PlayerController = Cast<ALivePreviewPlayerController>(Controller))
		{
			FVector delta(AutoRotateSpeed * DeltaSeconds, 0, 0);
			PlayerController->CameraOrbitByDelta(delta);
		}
	}
}


void AArcballPawn::OnMouseMoving(FVector Value)
{
	if (IgnoreInput)
		return;

	if (Value.SizeSquared() > 0.0f)
	{
		if (ALivePreviewPlayerController* PlayerController = Cast<ALivePreviewPlayerController>(Controller))
		{
			if (OrbitModifier)
			{
				// Restore raw values
				Value.X /= PlayerController->PlayerInput->GetMouseSensitivityX();
				Value.Y /= PlayerController->PlayerInput->GetMouseSensitivityY();

				PlayerController->CameraOrbitByDelta(Value);

				// show the indicator
				MeshIndicator->SetHiddenInGame(false, true);
				IndicatorHideTimeToGo = IndicatorHideTimer;
			}
			else if (OffsetZModifier)
			{
				CameraTargetOffset(-Value.X, -Value.Y);
			}
		}
	}
}


void AArcballPawn::OnMouseWheel(float Value)
{
	if (IgnoreInput)
		return;

	if (Value != 0.0f)
	{
		if (ALivePreviewPlayerController* PlayerController = Cast<ALivePreviewPlayerController>(Controller))
		{
			PlayerController->CameraZoomByDelta(-Value);

			// show the indicator
			MeshIndicator->SetHiddenInGame(false, true);
			IndicatorHideTimeToGo = IndicatorHideTimer;
		}
	}
}

void AArcballPawn::CameraZoomByPinch(float Pinch)
{
	if (ALivePreviewPlayerController* PlayerController = Cast<ALivePreviewPlayerController>(Controller))
	{
		PlayerController->CameraZoomByPercentage(Pinch);

		// show the indicator
		MeshIndicator->SetHiddenInGame(false, true);
		IndicatorHideTimeToGo = IndicatorHideTimer;
	}
}

void AArcballPawn::CameraTargetOffset(float X, float Z)
{
	if (IgnoreInput)
		return;

	if (X != 0.0f || Z != 0.0f)
	{
		if (ALivePreviewPlayerController* PlayerController = Cast<ALivePreviewPlayerController>(Controller))
		{
			check(PlayerController->PlayerCameraManager);

			PlayerController->CameraTargetOffsetByDelta(X, Z, true);

			// show the indicator
			MeshIndicator->SetHiddenInGame(false, true);
			IndicatorHideTimeToGo = IndicatorHideTimer;
		}
	}
}

void AArcballPawn::OnMouseRightButtonPressed()
{
	OffsetZModifier = true;
}

void AArcballPawn::OnMouseRightButtonReleased()
{
	OffsetZModifier = false;
}

void AArcballPawn::OnMouseLeftButtonPressed()
{
	if (IgnoreInput)
		return;

	if (ALivePreviewPlayerController* PlayerController = Cast<ALivePreviewPlayerController>(Controller))
	{
		float mouseX, mouseY;
		if (PlayerController->GetMousePosition(mouseX, mouseY))
		{
			LastMousePressLocation = FVector2D(mouseX, mouseY);
			LastMousePressTime = FPlatformTime::ToMilliseconds(FPlatformTime::Cycles());
			OrbitModifier = true;
		}

		PlayerController->HideMouseCursor();
	}
}

void AArcballPawn::OnMouseLeftButtonReleased()
{
	if (IgnoreInput)
		return;

	if (ALivePreviewPlayerController* PlayerController = Cast<ALivePreviewPlayerController>(Controller))
	{
		PlayerController->ShowMouseCursor();

//		ALivePreviewPlayerCameraManager* PlayerCameraManager = Cast<ALivePreviewPlayerCameraManager>(PlayerController->PlayerCameraManager);
//		check(PlayerCameraManager);
		check(PlayerController->PlayerCameraManager);

		float mouseX, mouseY;
		if (PlayerController->GetMousePosition(mouseX, mouseY))
		{
			
			float Time = FPlatformTime::ToMilliseconds(FPlatformTime::Cycles());
			FVector2D MouseLoc = FVector2D(mouseX, mouseY);

			if (!DisableMoving && Time - LastMousePressTime < 250.0f && (MouseLoc - LastMousePressLocation).SizeSquared() < 16.0f)
			{
				if (PlayerController->MoveToByDeprojectingScreenPosition(mouseX, mouseY))
				{
					// show the indicator
					MeshIndicator->SetHiddenInGame(false, true);
					IndicatorHideTimeToGo = IndicatorHideTimer;
				}
			}

			OrbitModifier = false;
		}
	}
}


void AArcballPawn::OnTouchBegan(ETouchIndex::Type index, FVector touchPosition)
{
	if (IgnoreInput)
		return;

	TouchRegistry[index] = TouchCount;

	auto it = TouchRegistry.find(index);
	if (it != TouchRegistry.end())
	{
		int ordinal = it->second;

		LastTouchBeginLocation[ordinal] = touchPosition;
		LastTouchMoveLocation[ordinal] = touchPosition;
		LastTouchBeginTime[ordinal] = FPlatformTime::ToMilliseconds(FPlatformTime::Cycles());
	}

	TouchCount = TouchRegistry.size();

	if (TouchCount >= 2)
	{
		if (ALivePreviewPlayerController* PlayerController = Cast<ALivePreviewPlayerController>(Controller))
		{
			PlayerController->CameraZoomSaveCurrent();
		}
	}
}

void AArcballPawn::OnTouchEnded(ETouchIndex::Type index, FVector touchPosition)
{
	if (IgnoreInput)
		return;

	if (TouchCount == 1)
	{
		if (ALivePreviewPlayerController* PlayerController = Cast<ALivePreviewPlayerController>(Controller))
		{
			int ordinal = TouchRegistry[index];
			float Time = FPlatformTime::ToMilliseconds(FPlatformTime::Cycles());
			if (!DisableMoving && Time - LastTouchBeginTime[ordinal] < 250.0f && (touchPosition - LastTouchBeginLocation[ordinal]).SizeSquared() < 16.0f)
			{
				if (PlayerController->MoveToByDeprojectingScreenPosition(touchPosition.X, touchPosition.Y))
				{
					// show the indicator
					MeshIndicator->SetHiddenInGame(false, true);
					IndicatorHideTimeToGo = IndicatorHideTimer;
				}
			}
		}
	}

	auto it = TouchRegistry.find(index);
	if (it != TouchRegistry.end())
	{
		TouchRegistry.erase(it);
	}

	TouchCount = TouchRegistry.size();
}


void AArcballPawn::OnTouchMoved(ETouchIndex::Type index, FVector touchPosition)
{
	if (IgnoreInput)
		return;

	if (TouchCount == 1)
	{
		// Single touch sweep, orbit the camera
		if (ALivePreviewPlayerController* PlayerController = Cast<ALivePreviewPlayerController>(Controller))
		{
			int ordinal = TouchRegistry[index];
			FVector Delta = touchPosition - LastTouchMoveLocation[ordinal];
			Delta.Y *= -1;

			PlayerController->CameraOrbitByDelta(Delta);

			LastTouchMoveLocation[ordinal] = touchPosition;

			// show the indicator
			MeshIndicator->SetHiddenInGame(false, true);
			IndicatorHideTimeToGo = IndicatorHideTimer;
		}
	}
	else if (TouchCount == 2)
	{
		FVector PrevMoveLocation[2] = {
			LastTouchMoveLocation[0],
			LastTouchMoveLocation[1]
		};


		LastTouchMoveLocation[TouchRegistry[index]] = touchPosition;

		if (ALivePreviewPlayerController* PlayerController = Cast<ALivePreviewPlayerController>(Controller))
		{
			// Two fingers gesture, pinch + offset along Z
			FVector PinchStart = (LastTouchBeginLocation[0] - LastTouchBeginLocation[1]);
			FVector PinchCurr = (LastTouchMoveLocation[0] - LastTouchMoveLocation[1]);

			float PinchStartMag = PinchStart.Size();
			float PinchCurrMag = PinchCurr.Size();
			PinchStartMag = FMath::Clamp<float>(PinchStartMag, 0.1f, PinchStartMag);
			PinchCurrMag = FMath::Clamp<float>(PinchCurrMag, 0.1f, PinchCurrMag);
			float Pinch = PinchCurrMag / PinchStartMag;

			
			float DragAlong = FVector::DotProduct(PinchStart.GetSafeNormal(), PinchCurr.GetSafeNormal());
			if (DragAlong > 0.8f)
			{
				// Gesture is pinch or offset Z
				// 
				if (Pinch > 1.2f || Pinch < 0.8f)
				{
//					GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, *FString::Printf(TEXT("Pinch: %.3f"), Pinch));
					PlayerController->CameraZoomByPercentage(1.0f / Pinch);
				}
				else
				{
					// calculate z offset
					FVector OffsetStart = (PrevMoveLocation[0] + PrevMoveLocation[1]) * 0.5f;
					FVector OffsetCurr = (LastTouchMoveLocation[0] + LastTouchMoveLocation[1]) * 0.5f;

					float OffsetX = -(OffsetCurr.X - OffsetStart.X);
					float OffsetY = OffsetCurr.Y - OffsetStart.Y;

//					GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, *FString::Printf(TEXT("Offset: %.3f"), Offset));
					PlayerController->CameraTargetOffsetByDelta(OffsetX, OffsetY, false);
				}
				
			}

			// show the indicator
			MeshIndicator->SetHiddenInGame(false, true);
			IndicatorHideTimeToGo = IndicatorHideTimer;

		}
	}
}
