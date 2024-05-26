/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-17 04:15:55
* @Last Modified by:   Ye Feng
* @Last Modified time: 2021-11-17 04:34:50
*/
#pragma once

#include "Modules/ModuleManager.h"
#include "IAssetTools.h"

DECLARE_LOG_CATEGORY_EXTERN(EvercoastEditorLog, Log, All);

class FEvercoastPlaybackEditorModule : public IModuleInterface
{
public:
	static inline FEvercoastPlaybackEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FEvercoastPlaybackEditorModule>("EvercoastPlaybackEditor");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("EvercoastPlaybackEditor");
	}

	static EAssetTypeCategories::Type GetEvercoastAssetCategory() {
		return EvercoastAssetCategory;
	}
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;


	virtual bool SupportsDynamicReloading() override;

private:
	void RegisterAssetTools();
	void RegisterCustomisations();
	void UnregisterAssetTools();
	void UnregisterCustomisations();
	void RegisterHelpMenuEntries();

	void ValidatePackageSettings();

	static EAssetTypeCategories::Type EvercoastAssetCategory;

	// Use weak pointer to allow hold weak reference, if AssetTools are destoryed before this module
	TArray<TWeakPtr<IAssetTypeActions>> RegisteredAssetTypeActions;

	FDelegateHandle TrackEditorBindingHandle;

	FName ECVAssetClassName;
};
