/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-17 02:28:37
* @Last Modified by:   Ye Feng
* @Last Modified time: 2022-04-25 10:28:56
*/

#include "EvercoastPlaybackModule.h"
#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "GhostTreeFormatReader.h"
#include "InstructionSet.h"
#include "EvercoastDecoder.h"
#include "picoquic.h"
#include "Realtime/EvercoastRealtimeConfig.h"
#include "ec_decoder_compatibility.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformProcess.h"
#elif (PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS)
#else
#error PicoQuic unsupported platform!
#endif

#define LOCTEXT_NAMESPACE "FEvercoastPlaybackModule"

bool g_SupportDecodeVoxel = false;

static void RemoveAllDiskCacheFiles()
{
	IFileManager& FileManager = IFileManager::Get();
	const TCHAR* CacheDir = FGenericPlatformMisc::GamePersistentDownloadDir();
	TArray<FString> Files;
	FString Wildcards = FString::Printf(TEXT("%s*%s"), GHOSTTREE_DISKCACHE_PREFIX, GHOSTTREE_DISKCACHE_EXTENSION);
	Wildcards = FPaths::Combine(CacheDir, Wildcards);
	FileManager.FindFiles(Files, *Wildcards, true, false);
	for (const auto& Filename : Files)
	{
		FileManager.Delete(*FPaths::Combine(CacheDir, Filename), false, false, true);
	}
}


void FEvercoastPlaybackModule::StartupModule()
{
	// Get the base directory of this plugin
	FString BaseDir = IPluginManager::Get().FindPlugin("EvercoastPlayback")->GetBaseDir();

	// Maps virtual shader source directory to actual shaders directory on disk.
	FString ShaderDirectory = FPaths::Combine(BaseDir, TEXT("Source/EvercoastPlayback/Shaders/"));
	AddShaderSourceDirectoryMapping("/EvercoastShaders", ShaderDirectory);

	// Delete all the disk cache files in the persistent download dir
	RemoveAllDiskCacheFiles();

#ifdef GT_CHECK_INNSTRUCTION_SET
	g_SupportDecodeVoxel = InstructionSet::AVX2();
#else
	g_SupportDecodeVoxel = true;
#endif
	if (g_SupportDecodeVoxel)
	{
		initialise_decoding_api();
	}
	else
	{
		UE_LOG(EvercoastDecoderLog, Error, TEXT("This desktop CPU doesn't support AVX2 instruction set. ECV files won't be able to playback!"));
	}

	// This plugin is loaded earlier, so UEvercoastRealtimeConfig has to be configured after UObject's initialisation
	FCoreDelegates::OnFEngineLoopInitComplete.AddRaw(this, &FEvercoastPlaybackModule::SetupPicoQuicLibrary);
}

void FEvercoastPlaybackModule::ShutdownModule()
{
#if PLATFORM_WINDOWS
	if (PicoQuicDllHandle)
	{
		FPlatformProcess::FreeDllHandle(PicoQuicDllHandle);
		PicoQuicDllHandle = nullptr;
	}
#endif

	if (g_SupportDecodeVoxel)
	{
		deinitialise_decoding_api();
	}

	RemoveAllDiskCacheFiles();
}

bool FEvercoastPlaybackModule::SupportsDynamicReloading()
{
	return false;
}

void FEvercoastPlaybackModule::SetupPicoQuicLibrary()
{
#if PLATFORM_WINDOWS
#if WITH_EDITOR
	FString DllDirectory = FPaths::Combine(IPluginManager::Get().FindPlugin("EvercoastPlayback")->GetBaseDir(), TEXT("Binaries/Win64"));
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

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FEvercoastPlaybackModule, EvercoastPlayback)
