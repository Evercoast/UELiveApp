/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-16 03:13:50
* @Last Modified by:   Ye Feng
* @Last Modified time: 2021-11-17 04:48:05
*/
#include "EvercoastECVAssetFactoryNew.h"
#include "EvercoastECVAsset.h"
#include "EvercoastPlaybackEditorModule.h"

UEvercoastECVAssetFactoryNew::UEvercoastECVAssetFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UEvercoastECVAsset::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UEvercoastECVAssetFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UEvercoastECVAsset* ecvAsset;

	ecvAsset = NewObject<UEvercoastECVAsset>(InParent, InClass, InName, Flags);
	return ecvAsset;
}

uint32 UEvercoastECVAssetFactoryNew::GetMenuCategories() const
{
	return FEvercoastPlaybackEditorModule::GetEvercoastAssetCategory();
}

bool UEvercoastECVAssetFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
