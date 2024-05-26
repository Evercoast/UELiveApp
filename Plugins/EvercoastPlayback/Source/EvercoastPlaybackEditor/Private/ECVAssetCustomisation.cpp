
#include "ECVAssetCustomisation.h"

#include "EditorStyleSet.h"
#include "Modules/ModuleManager.h"
#include "PlatformInfo.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"

#include "EvercoastECVAsset.h"

#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layout/Margin.h"
#include "Textures/SlateIcon.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ECVAssetCustomisation"



/* IDetailCustomization interface
 *****************************************************************************/

void FECVAssetCustomisation::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// customize 'Platforms' category
	IDetailCategoryBuilder& OverridesCategory = DetailBuilder.EditCategory("Platforms");
	{
		// PlatformPlayerNames
		PlatformCookingOverrideProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UEvercoastECVAsset, PlatformCookOverride));
		{
			IDetailPropertyRow& PlayerNamesRow = OverridesCategory.AddProperty(PlatformCookingOverrideProperty);

			PlayerNamesRow
				.ShowPropertyButtons(false)
				.CustomWidget()
				.NameContent()
				[
					PlatformCookingOverrideProperty->CreatePropertyNameWidget()
				]
			.ValueContent()
				.MaxDesiredWidth(0.0f)
				[
					MakePlatformCookOverrideValueWidget()
				];
		}
	}
}


TSharedRef<SWidget> FECVAssetCustomisation::MakePlatformCookOverrideValueWidget()
{
#if ENGINE_MAJOR_VERSION == 5
	// get available platforms
	TArray<const PlatformInfo::FTargetPlatformInfo*> AvailablePlatforms;

	for (const PlatformInfo::FTargetPlatformInfo* PlatformInfo : PlatformInfo::GetPlatformInfoArray())
	{
		if (PlatformInfo->IsVanilla() && (PlatformInfo->PlatformType == EBuildTargetType::Game) && (PlatformInfo->Name != TEXT("AllDesktop")))
		{
			AvailablePlatforms.Add(PlatformInfo);
		}
	}

	AvailablePlatforms.Sort([](const PlatformInfo::FTargetPlatformInfo& One, const PlatformInfo::FTargetPlatformInfo& Two) -> bool
		{
			return One.DisplayName.CompareTo(Two.DisplayName) < 0;
		});

	// build value widget
	TSharedRef<SGridPanel> PlatformPanel = SNew(SGridPanel);

	for (int32 PlatformIndex = 0; PlatformIndex < AvailablePlatforms.Num(); ++PlatformIndex)
	{
		const PlatformInfo::FTargetPlatformInfo* Platform = AvailablePlatforms[PlatformIndex];
		FString PlatformName = Platform->IniPlatformName.ToString();

		// platform icon
		PlatformPanel->AddSlot(0, PlatformIndex)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
#if ENGINE_MINOR_VERSION >= 1
				.Image(FAppStyle::GetBrush(Platform->GetIconStyleName(EPlatformIconSize::Normal)))
#else
				.Image(FEditorStyle::GetBrush(Platform->GetIconStyleName(EPlatformIconSize::Normal)))
#endif
			];

		// platform name
		PlatformPanel->AddSlot(1, PlatformIndex)
			.Padding(4.0f, 0.0f, 16.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Platform->DisplayName)
			];

		// player combo box
		PlatformPanel->AddSlot(2, PlatformIndex)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([&, _PlatformName = PlatformName]()
					{
						TArray<UObject*> OuterObjects;
						{
							PlatformCookingOverrideProperty->GetOuterObjects(OuterObjects);
						}

						if (OuterObjects.Num() == 0)
							return ECheckBoxState::Checked;

						bool* pRequiresCooking = Cast<UEvercoastECVAsset>(OuterObjects[0])->PlatformCookOverride.Find(_PlatformName);
						if (!pRequiresCooking)
						{
							return ECheckBoxState::Checked;
						}
						else
						{
							return *pRequiresCooking ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						}
					})
			.OnCheckStateChanged_Lambda([&, _PlatformName = PlatformName](const ECheckBoxState NewState)
				{
					TArray<UObject*> OuterObjects;
					{
						PlatformCookingOverrideProperty->GetOuterObjects(OuterObjects);
					}

					if (OuterObjects.Num() == 0)
						return;

					for (auto* Object : OuterObjects)
					{
						UEvercoastECVAsset* ecvAsset = Cast<UEvercoastECVAsset>(Object);
						bool hasOldValue = ecvAsset->PlatformCookOverride.Contains(_PlatformName);
						bool& requiresCooking = ecvAsset->PlatformCookOverride.FindOrAdd(_PlatformName);
						bool newRequiresCooking = (NewState == ECheckBoxState::Checked);

						if (!hasOldValue || requiresCooking != newRequiresCooking)
						{
							requiresCooking = newRequiresCooking;
							ecvAsset->Modify(true);
						}
					}
				})
			];
	}

	return PlatformPanel;
#else
	// get available platforms
	TArray<const PlatformInfo::FPlatformInfo*> AvailablePlatforms;

	for (const PlatformInfo::FPlatformInfo& PlatformInfo : PlatformInfo::GetPlatformInfoArray())
	{
		if (PlatformInfo.IsVanilla() && (PlatformInfo.PlatformType == EBuildTargetType::Game) && (PlatformInfo.PlatformInfoName != TEXT("AllDesktop")))
		{
			AvailablePlatforms.Add(&PlatformInfo);
		}
	}

	AvailablePlatforms.Sort([](const PlatformInfo::FPlatformInfo& One, const PlatformInfo::FPlatformInfo& Two) -> bool
		{
			return One.DisplayName.CompareTo(Two.DisplayName) < 0;
		});

	// build value widget
	TSharedRef<SGridPanel> PlatformPanel = SNew(SGridPanel);

	for (int32 PlatformIndex = 0; PlatformIndex < AvailablePlatforms.Num(); ++PlatformIndex)
	{
		const PlatformInfo::FPlatformInfo* Platform = AvailablePlatforms[PlatformIndex];
		FString PlatformName = Platform->IniPlatformName;

		// platform icon
		PlatformPanel->AddSlot(0, PlatformIndex)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush(Platform->GetIconStyleName(PlatformInfo::EPlatformIconSize::Normal)))
			];

		// platform name
		PlatformPanel->AddSlot(1, PlatformIndex)
			.Padding(4.0f, 0.0f, 16.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Platform->DisplayName)
			];

		// player combo box
		PlatformPanel->AddSlot(2, PlatformIndex)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([&, _PlatformName=PlatformName]()
				{
					TArray<UObject*> OuterObjects;
					{
						PlatformCookingOverrideProperty->GetOuterObjects(OuterObjects);
					}

					if (OuterObjects.Num() == 0)
						return ECheckBoxState::Checked;

					bool* pRequiresCooking = Cast<UEvercoastECVAsset>(OuterObjects[0])->PlatformCookOverride.Find(_PlatformName);
					if (!pRequiresCooking)
					{
						return ECheckBoxState::Checked;
					}
					else
					{
						return *pRequiresCooking ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}
				})
				.OnCheckStateChanged_Lambda([&, _PlatformName = PlatformName](const ECheckBoxState NewState)
				{
					TArray<UObject*> OuterObjects;
					{
						PlatformCookingOverrideProperty->GetOuterObjects(OuterObjects);
					}

					if (OuterObjects.Num() == 0)
						return;

					for (auto* Object : OuterObjects)
					{
						UEvercoastECVAsset* ecvAsset = Cast<UEvercoastECVAsset>(Object);
						bool hasOldValue = ecvAsset->PlatformCookOverride.Contains(_PlatformName);
						bool& requiresCooking = ecvAsset->PlatformCookOverride.FindOrAdd(_PlatformName);
						bool newRequiresCooking = (NewState == ECheckBoxState::Checked);

						if (!hasOldValue || requiresCooking != newRequiresCooking)
						{
							requiresCooking = newRequiresCooking;
							ecvAsset->Modify(true);
						}
					}
				})
			];
	}

	return PlatformPanel;
#endif
}

#undef LOCTEXT_NAMESPACE
