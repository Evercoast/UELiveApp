// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvercoastRealtimeModule.h"
#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "picoquic.h"
#include "EvercoastRealtimeConfig.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformProcess.h"
#elif (PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS)
#else
#error PicoQuic unsupported platform!
#endif

#define LOCTEXT_NAMESPACE "FEvercoastRealtimeModule"


void FEvercoastRealtimeModule::SetupPicoQuicLibrary()
{
#if PLATFORM_WINDOWS
#if WITH_EDITOR
	FString DllDirectory = FPaths::Combine(IPluginManager::Get().FindPlugin("EvercoastRealtime")->GetBaseDir(), TEXT("Binaries/Win64"));
#else
	FString DllDirectory = FString(FPlatformProcess::BaseDir());
#endif

	UEvercoastRealtimeConfig* config = NewObject<UEvercoastRealtimeConfig>();
	if (config->UseOldPicoQuic)
	{
		PicoQuicDllHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine(DllDirectory, TEXT("picoquicclient_old.dll")));
	}
	else
	{
		PicoQuicDllHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine(DllDirectory, TEXT("picoquicclient.dll")));
	}

	
	if (PicoQuicDllHandle)
	{
		using namespace PicoQuic;
		create_connection = (vci_create_connection)FPlatformProcess::GetDllExport(PicoQuicDllHandle, TEXT("vci_create_connection"));
		create_connection_2 = (vci_create_connection_2)FPlatformProcess::GetDllExport(PicoQuicDllHandle, TEXT("vci_create_connection_2"));
		
		if (!config->UseOldPicoQuic)
			reconnect = (vci_reconnect)FPlatformProcess::GetDllExport(PicoQuicDllHandle, TEXT("vci_reconnect"));

		delete_connection = (vci_delete_connection)FPlatformProcess::GetDllExport(PicoQuicDllHandle, TEXT("vci_delete_connection"));
		get_status = (vci_get_status)FPlatformProcess::GetDllExport(PicoQuicDllHandle, TEXT("vci_get_status"));
		received_frame = (vci_received_frame)FPlatformProcess::GetDllExport(PicoQuicDllHandle, TEXT("vci_received_frame"));
		get_data_size = (vci_get_data_size)FPlatformProcess::GetDllExport(PicoQuicDllHandle, TEXT("vci_get_data_size"));
		get_user_data_size = (vci_get_user_data_size)FPlatformProcess::GetDllExport(PicoQuicDllHandle, TEXT("vci_get_user_data_size"));
		get_frame_number = (vci_get_frame_number)FPlatformProcess::GetDllExport(PicoQuicDllHandle, TEXT("vci_get_frame_number"));
		get_timestamp = (vci_get_timestamp)FPlatformProcess::GetDllExport(PicoQuicDllHandle, TEXT("vci_get_timestamp"));
		get_type_and_flags = (vci_get_type_and_flags)FPlatformProcess::GetDllExport(PicoQuicDllHandle, TEXT("vci_get_type_and_flags"));
		get_data = (vci_get_data)FPlatformProcess::GetDllExport(PicoQuicDllHandle, TEXT("vci_get_data"));
		get_user_data = (vci_get_user_data)FPlatformProcess::GetDllExport(PicoQuicDllHandle, TEXT("vci_get_user_data"));
		pop_frame = (vci_pop_frame)FPlatformProcess::GetDllExport(PicoQuicDllHandle, TEXT("vci_pop_frame"));
		update_framerate = (vci_update_framerate)FPlatformProcess::GetDllExport(PicoQuicDllHandle, TEXT("vci_update_framerate"));
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Library Loading Error", "Failed to load PicoQuic library"));
	}
#elif (PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS)
	// Should just work with vci_* functions
#else
	check(false && "Only Win64 and Android are supported for now.");
#endif
}

void FEvercoastRealtimeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	SetupPicoQuicLibrary();
}

void FEvercoastRealtimeModule::ShutdownModule()
{
#if PLATFORM_WINDOWS
	if (PicoQuicDllHandle)
	{
		FPlatformProcess::FreeDllHandle(PicoQuicDllHandle);
		PicoQuicDllHandle = nullptr;
	}
#endif
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FEvercoastRealtimeModule, EvercoastRealtime)
