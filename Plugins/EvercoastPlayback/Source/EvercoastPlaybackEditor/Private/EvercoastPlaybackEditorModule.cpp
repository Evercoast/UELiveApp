/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-17 04:24:53
* @Last Modified by:   feng_ye
* @Last Modified time: 2024-05-02 04:03:41
*/
#include "EvercoastPlaybackEditorModule.h"
#include "AssetToolsModule.h"
#include "EvercoastAssetActions.h"
#include "EvercoastConstants.h"
#include "ToolMenus.h"
#include "Settings/ProjectPackagingSettings.h"
#include "LevelEditor.h"
#include "ISequencerModule.h"
#include "ECVAssetTrackEditor.h"
#include "EvercoastECVAsset.h"
#include "ECVAssetCustomisation.h"


DEFINE_LOG_CATEGORY(EvercoastEditorLog);
EAssetTypeCategories::Type FEvercoastPlaybackEditorModule::EvercoastAssetCategory;


void FEvercoastPlaybackEditorModule::StartupModule()
{
	ECVAssetClassName = UEvercoastECVAsset::StaticClass()->GetFName();

	if (!IsRunningCommandlet() && !IsRunningGame() && FSlateApplication::IsInitialized())
	{
		FLevelEditorModule* LevelEditor = FModuleManager::LoadModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
		if (LevelEditor)
		{
			RegisterHelpMenuEntries();
		}
	}

	RegisterAssetTools();
	RegisterCustomisations();

	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
	TrackEditorBindingHandle = SequencerModule.RegisterPropertyTrackEditor<FECVAssetTrackEditor>();

	// Force adding EvercoastVolcapSource directory to "Always Staging UFS Directory" for less packaging frustration
	ValidatePackageSettings();
}

void FEvercoastPlaybackEditorModule::ShutdownModule()
{
	ISequencerModule* SequencerModulePtr = FModuleManager::Get().GetModulePtr<ISequencerModule>("Sequencer");
	if (SequencerModulePtr)
	{
		SequencerModulePtr->UnRegisterTrackEditor(TrackEditorBindingHandle);
	}

	UnregisterCustomisations();
	UnregisterAssetTools();
}

bool FEvercoastPlaybackEditorModule::SupportsDynamicReloading()
{
	return false;
}


void FEvercoastPlaybackEditorModule::RegisterAssetTools()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	EvercoastAssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("Evercoast")), FText::FromString(TEXT("Evercoast")));

	auto action = MakeShared<FEvercoastAssetActions>();
	AssetTools.RegisterAssetTypeActions(action);
	RegisteredAssetTypeActions.Add(action);

}

void FEvercoastPlaybackEditorModule::RegisterCustomisations()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	{
		PropertyModule.RegisterCustomClassLayout(ECVAssetClassName, FOnGetDetailCustomizationInstance::CreateStatic(&FECVAssetCustomisation::MakeInstance));
	}
}

void FEvercoastPlaybackEditorModule::UnregisterAssetTools()
{
	FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");
	if (AssetToolsModule != nullptr)
	{
		IAssetTools& AssetTools = AssetToolsModule->Get();

		for (auto weakAction : RegisteredAssetTypeActions)
		{
			auto action = weakAction.Pin();
			AssetTools.UnregisterAssetTypeActions(action.ToSharedRef());
		}
	}
	else
	{
		RegisteredAssetTypeActions.Empty();
	}
}

void FEvercoastPlaybackEditorModule::UnregisterCustomisations()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	{
		PropertyModule.UnregisterCustomClassLayout(ECVAssetClassName);
	}
}

void FEvercoastPlaybackEditorModule::RegisterHelpMenuEntries()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* HelpMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Help");
	FToolMenuSection& Section = HelpMenu->AddSection("EvercoastHelp", NSLOCTEXT("EvercoastText", "EvercoastHelpLabel", "Evercoast Help"),
		FToolMenuInsert("HelpBrowse", EToolMenuInsertType::Default));

	Section.AddEntry(FToolMenuEntry::InitMenuEntry(
		NAME_None,
		NSLOCTEXT("EvercoastText", "EvercoastEnsurePackaging", "Ensure Packaging Settings for Evercoast"),
		NSLOCTEXT("EvercoastText", "EvercoastEnsurePackagingTooltip", "Verifies the settings in Packaging is compatbile with Evercoast's content."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FEvercoastPlaybackEditorModule::ValidatePackageSettings))
	));
}

void FEvercoastPlaybackEditorModule::ValidatePackageSettings()
{
	bool bEvercoastCookDirectoryFound = false;
	UProjectPackagingSettings* PackagingSettings = Cast<UProjectPackagingSettings>(UProjectPackagingSettings::StaticClass()->GetDefaultObject());
	for (int i = 0; i < PackagingSettings->DirectoriesToAlwaysStageAsUFS.Num(); ++i)
	{
		if (PackagingSettings->DirectoriesToAlwaysStageAsUFS[i].Path == EVERCOAST_VOLCAP_SOURCE_DATA_DIRECTORY_REL_CONTENT)
		{
			bEvercoastCookDirectoryFound = true;
			break;
		}
	}

	if (!bEvercoastCookDirectoryFound)
	{
		UE_LOG(EvercoastEditorLog, Warning, TEXT("Evercoast volcap source directory is not set in the 'Additional Non-Asset Directories to Package' in packaging settings. Force adding it now."));
		FDirectoryPath newAlwaysStagePath;
		newAlwaysStagePath.Path = EVERCOAST_VOLCAP_SOURCE_DATA_DIRECTORY_REL_CONTENT;
		PackagingSettings->DirectoriesToAlwaysStageAsUFS.Add(newAlwaysStagePath);
#if ENGINE_MAJOR_VERSION == 5
		PackagingSettings->TryUpdateDefaultConfigFile();
#else
		PackagingSettings->UpdateDefaultConfigFile();
#endif
	}
	else
	{
		UE_LOG(EvercoastEditorLog, Log, TEXT("Evercoast cook directory is properly set in the packaging settings. Sweet."));
	}

	FString absolutePluginPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir());
	FPaths::MakePathRelativeTo(absolutePluginPath, *FPaths::ProjectContentDir());

	FString realtimeCryptoDir = FPaths::Combine(absolutePluginPath, TEXT("EvercoastPlayback/Content/Crypto"));
	bool bEvercoastCryptoDirectoryFound = false;
	for (int i = 0; i < PackagingSettings->DirectoriesToAlwaysStageAsUFS.Num(); ++i)
	{
		if (PackagingSettings->DirectoriesToAlwaysStageAsUFS[i].Path == realtimeCryptoDir)
		{
			bEvercoastCryptoDirectoryFound = true;
			break;
		}
	}

	if (!bEvercoastCryptoDirectoryFound)
	{
		UE_LOG(EvercoastEditorLog, Warning, TEXT("Evercoast realtime crypto directory is not set in the 'Additional Non-Asset Directories to Copy' in packaging settings. Force adding it now."));

		FDirectoryPath newAlwaysStagePath;
		newAlwaysStagePath.Path = realtimeCryptoDir;
		PackagingSettings->DirectoriesToAlwaysStageAsUFS.Add(newAlwaysStagePath);
#if ENGINE_MAJOR_VERSION == 5
		PackagingSettings->TryUpdateDefaultConfigFile();
#else
		PackagingSettings->UpdateDefaultConfigFile();
#endif
	}
}

IMPLEMENT_MODULE(FEvercoastPlaybackEditorModule, EvercoastPlaybackEditor)