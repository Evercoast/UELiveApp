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
#include "ec_decoder_compatibility.h"
#include "GhostTreeFormatReader.h"
#include "InstructionSet.h"
#include "EvercoastDecoder.h"

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
}

void FEvercoastPlaybackModule::ShutdownModule()
{
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


#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FEvercoastPlaybackModule, EvercoastPlayback)
