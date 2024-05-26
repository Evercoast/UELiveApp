// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FEvercoastRealtimeModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

	void SetupPicoQuicLibrary();
	/** Handle to the test dll we will load */
	void* PicoQuicDllHandle;
};
