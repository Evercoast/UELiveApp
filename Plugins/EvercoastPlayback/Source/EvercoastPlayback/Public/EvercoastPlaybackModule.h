// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FEvercoastPlaybackModule : public IModuleInterface
{
public:
	static inline FEvercoastPlaybackModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FEvercoastPlaybackModule>("EvercoastPlayback");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("EvercoastPlayback");
	}

public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;


	virtual bool SupportsDynamicReloading() override;

private:
};
