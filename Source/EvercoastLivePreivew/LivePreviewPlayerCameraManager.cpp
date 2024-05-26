#include "LivePreviewPlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "LivePreviewPlayerController.h"
#include <algorithm>

ALivePreviewPlayerCameraManager::ALivePreviewPlayerCameraManager(const FObjectInitializer& Initializer) :
	Super(Initializer)
{
	LocationBlendDuration = 1.0f;
	LocationBlendTimeToGo = -1.0f;

	SweepingCameraRadius = 15.0f;
	CameraPositionLimitNegativeZ = 0;

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComp"));
	CapsuleComponent->InitCapsuleSize(30.0f, 30.0f);
	CapsuleComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);

	CapsuleComponent->CanCharacterStepUpOn = ECB_No;
	CapsuleComponent->SetShouldUpdatePhysicsVolume(true);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	CapsuleComponent->bDynamicObstacle = true;

	CapsuleComponent->SetupAttachment(GetTransformComponent());
}

void ALivePreviewPlayerCameraManager::SaveCurrentCameraPOV()
{
	SavedCameraPOV = CurrCameraPOV;
	SavedFreeCamOffset = FreeCamOffset;
	SavedFreeCamDist = FreeCamDistance;

	LocationBlendTimeToGo = LocationBlendDuration;
	LocationBlendParams.BlendTime = LocationBlendDuration;
}

bool ALivePreviewPlayerCameraManager::IsLocationBlendFinished() const
{
	return LocationBlendTimeToGo <= 0.0f;
}

void ALivePreviewPlayerCameraManager::UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime)
{
	if (OutVT.Target)
	{
		if (CameraStyle == TEXT("Arcball"))
		{

			// Simple third person view implementation
			FVector Loc = OutVT.Target->GetActorLocation();
			FRotator Rotator = OutVT.Target->GetActorRotation();

			if (OutVT.Target == PCOwner)
			{
				Loc = PCOwner->GetFocalLocation();
			}

			Rotator = PCOwner->GetControlRotation();

			FVector Pos = Loc + ViewTargetOffset + FRotationMatrix(Rotator).TransformVector(FreeCamOffset) - Rotator.Vector() * FreeCamDistance;

			ALivePreviewPlayerController* thePlayerController = Cast<ALivePreviewPlayerController>(PCOwner);
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FreeCam), false, this);
			QueryParams.AddIgnoredActor(OutVT.Target);
			QueryParams.AddIgnoredActor(this);
			QueryParams.AddIgnoredActor(IgnoredPawn);
			for (auto* SurfaceActor : thePlayerController->WalkingSurfaceActors)
			{
				QueryParams.AddIgnoredActor(SurfaceActor);
			}
			FHitResult Result;

			FVector FinalLocation;
			GetWorld()->SweepSingleByChannel(Result, Loc, Pos, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(SweepingCameraRadius), QueryParams);
			if (Result.bBlockingHit)
			{
				FinalLocation = Result.Location;
			}
			else
			{
				FinalLocation = Pos;
			}

			if (FinalLocation.Z < CameraPositionLimitNegativeZ)
				FinalLocation.Z = CameraPositionLimitNegativeZ;

			

			// interpolate current pawn location to the final localtion
			if (LocationBlendTimeToGo > 0.0f)
			{
				LocationBlendTimeToGo -= DeltaTime;
				LocationBlendTimeToGo = std::max(0.0f, LocationBlendTimeToGo);

				float DurationPct = (LocationBlendParams.BlendTime - LocationBlendTimeToGo) / LocationBlendParams.BlendTime;
				float A = LocationBlendParams.GetBlendAlpha(DurationPct);

				OutVT.POV.Location = (1.0f - A) * SavedCameraPOV.Location + A * FinalLocation;

//				FreeCamOffset = (1.0f - A) * SavedFreeCamOffset + A * FVector(0, 0, thePlayerController->InitialCameraOffsetZ);
//				FreeCamDistance = (1.0f - A) * SavedFreeCamDist + A * thePlayerController->InitialCameraDistance;
			}
			else
			{
				if (LocationBlendTimeToGo < 0)
				{
					LocationBlendTimeToGo = 0;
					
					FreeCamOffset = FVector(0, 0, thePlayerController->InitialCameraOffsetZ);
					FreeCamDistance = thePlayerController->InitialCameraDistance;
				}

				OutVT.POV.Location = FinalLocation;
			}

			
			
			OutVT.POV.Rotation = Rotator;

			CurrCameraPOV = OutVT.POV;
		}

	}
}

void ALivePreviewPlayerCameraManager::UpdatePawnReference(APawn* pawn)
{
	IgnoredPawn = pawn;
}