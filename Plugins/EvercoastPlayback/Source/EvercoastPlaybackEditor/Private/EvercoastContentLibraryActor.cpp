// Copyright (C) 2021 Evercoast Inc. All Rights Reserved.


#include "EvercoastContentLibraryActor.h"
#include "Engine/Selection.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Components/BillboardComponent.h"
#include "UObject/ConstructorHelpers.h"

// Sets default values
AEvercoastContentLibraryActor::AEvercoastContentLibraryActor()
{
	DefaultRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRootComponent"));
	EditorSprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("EditorSprite"));

	RootComponent = DefaultRootComponent;

	PrimaryActorTick.bCanEverTick = false;
	bHasOpenedURL = false;
	bHasLoadedFromLevel = false;

	if (EditorSprite)
	{
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> EvercoastLibraryTextureObject(TEXT("/EvercoastPlayback/EditorResources/EvercoastContentLibrary_Icon"));

		struct FConstructorStatics
		{
			FName ID_Sprite;
			FText NAME_Sprite;
			FConstructorStatics()
				: ID_Sprite(TEXT("EvercoastContentLibraryActor"))
				, NAME_Sprite(NSLOCTEXT("SpriteCategory", "EvercoastContentLibraryText", "Find Content at Evercoast"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;


		EditorSprite->Sprite = EvercoastLibraryTextureObject.Get();
		EditorSprite->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
		EditorSprite->SpriteInfo.Category = ConstructorStatics.ID_Sprite;
		EditorSprite->SpriteInfo.DisplayName = ConstructorStatics.NAME_Sprite;

		EditorSprite->bIsScreenSizeScaled = true;
		EditorSprite->SetupAttachment(RootComponent);
	}


	USelection::SelectObjectEvent.AddUObject(this, &AEvercoastContentLibraryActor::OnSelectionChanged);
}

// Called when the game starts or when spawned
void AEvercoastContentLibraryActor::BeginPlay()
{
	Super::BeginPlay();
	
}

void AEvercoastContentLibraryActor::PostLoad()
{
	Super::PostLoad();

	bHasLoadedFromLevel = true;

}

static const TCHAR* ContentLibraryURL = TEXT("https://support.evercoast.com/docs/unreal-engine-content-download-and-importing-guide");

void AEvercoastContentLibraryActor::OpenEvercoastLibrary()
{
	if (FPlatformProcess::CanLaunchURL(ContentLibraryURL))
	{
		FPlatformProcess::LaunchURL(ContentLibraryURL, nullptr, nullptr);
	}
}

void AEvercoastContentLibraryActor::OnSelectionChanged(UObject* Object)
{
	if (Object == this)
	{
		if (FPlatformProcess::CanLaunchURL(ContentLibraryURL) && bHasLoadedFromLevel && !bHasOpenedURL)
		{
			FPlatformProcess::LaunchURL(ContentLibraryURL, nullptr, nullptr);
			bHasOpenedURL = true;
		}
	}

}

