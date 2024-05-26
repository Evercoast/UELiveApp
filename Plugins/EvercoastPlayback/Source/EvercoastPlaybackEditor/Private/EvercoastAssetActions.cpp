/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-16 03:34:38
* @Last Modified by:   Ye Feng
* @Last Modified time: 2021-12-07 03:09:00
*/
#include "EvercoastAssetActions.h"
#include "EvercoastECVAsset.h"
#include "EvercoastPlaybackEditorModule.h"

uint32 FEvercoastAssetActions::GetCategories()
{
	return FEvercoastPlaybackEditorModule::GetEvercoastAssetCategory();
}


FText FEvercoastAssetActions::GetName() const
{
	return FText::FromString(TEXT("Evercoast ECV Asset"));
}


UClass* FEvercoastAssetActions::GetSupportedClass() const
{
	return UEvercoastECVAsset::StaticClass();
}

FColor FEvercoastAssetActions::GetTypeColor() const
{
	return FColor::Blue;
}

bool FEvercoastAssetActions::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}

bool FEvercoastAssetActions::CanFilter()
{
	return true;
}

void FEvercoastAssetActions::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto ecvAssetList = GetTypedWeakObjectPtrs<UEvercoastECVAsset>(InObjects);

	MenuBuilder.AddMenuEntry(
		FText::FromString("Force Cooking Content"),
		FText::FromString("Force to cook content to disk before packaging."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([ecvAssetList] {
				for (auto& ecvAsset : ecvAssetList)
				{
					if (ecvAsset.IsValid())
					{
						if (!ecvAsset->ValidateDataURL())
						{
							UE_LOG(EvercoastEditorLog, Error, TEXT("Validate data URL %s : FAILED"), *ecvAsset->DataURL);
						}
						ecvAsset->Cook();
					}
				}
			}),
			FCanExecuteAction::CreateLambda([] {
				return true;
			})
		)
	);

	MenuBuilder.AddMenuEntry(
		FText::FromString("Validate Content"),
		FText::FromString("Validate the Unreal asset created out of Evercoast .ecv content."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([ecvAssetList] {
				bool allValid = true;
				for (auto& ecvAsset : ecvAssetList)
				{
					if (!ecvAsset->IsEmpty())
					{
						if (ecvAsset->ValidateDataURL())
						{
							UE_LOG(EvercoastEditorLog, Log, TEXT("Validate data URL %s : PASSED"), *ecvAsset->DataURL);
						}
						else
						{
							UE_LOG(EvercoastEditorLog, Error, TEXT("Validate data URL %s : FAILED"), *ecvAsset->DataURL);
							allValid = false;
						}
					}
					else
					{
						UE_LOG(EvercoastEditorLog, Warning, TEXT("Asset %s has empty URL. Ignored"), *ecvAsset->GetFName().ToString());
					}
				}

				if (!allValid)
				{
					FMessageDialog::Open(EAppMsgType::Type::Ok, 
						FText::FromString(TEXT("Some cooked content checking failed. Please refer to the output log.")));
				}

			}),
			FCanExecuteAction::CreateLambda([] {
				return true;
				})
			)
	);

	MenuBuilder.AddMenuEntry(
		FText::FromString("Force Invalidate Content"),
		FText::FromString("Force invalidate the Unreal asset created out of Evercoast .ecv content. Use this when you changed the location of local or remote .ecv"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([ecvAssetList] {
				for (auto& ecvAsset : ecvAssetList)
				{
					ecvAsset->ForceInvalidateFlags();
				}

				}),
			FCanExecuteAction::CreateLambda([] {
					return true;
				})
			)
	);
}