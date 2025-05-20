/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-17 02:28:37
* @Last Modified by:   feng_ye
* @Last Modified time: 2025-03-25 13:41:11
*/
using System;
using System.IO;
using UnrealBuildTool;
using System.Collections.Generic;

public class EvercoastUnsupportedPlatformException : Exception
{

}

public class EvercoastPlayback : ModuleRules
{
	public string ProjectRoot
	{
		get
		{
			return System.IO.Path.GetFullPath(
				System.IO.Path.Combine(ModuleDirectory, "..", "..", "..", "..")
			);
		}
	}

	public string PluginsRoot
	{
		get
		{
			return System.IO.Path.GetFullPath(
				System.IO.Path.Combine(ModuleDirectory, "..", "..", "..")
			);
		}
	}

	public string ThisPluginRoot
	{
		get
		{
			return System.IO.Path.GetFullPath(
				System.IO.Path.Combine(ModuleDirectory, "..", "..")
			);
		}
	}

	public string ThirdPartyRoot
	{
		get
		{
			return System.IO.Path.GetFullPath(
				System.IO.Path.Combine(ModuleDirectory, "../ThirdParty/")
			);
		}
	}

	// Process all files in the directory passed in, recurse on any directories 
	// that are found, and process the files they contain.
	public void AddDependencyProcessDirectoryRecursive(string targetDirectory, IList<string> addToList)
	{
		// Process the list of files found in the directory.
		string[] fileEntries = Directory.GetFiles(targetDirectory);
		foreach (string fileName in fileEntries)
			AddDependencyProcessFile(fileName, addToList);

		// Recurse into subdirectories of this directory.
		string[] subdirectoryEntries = Directory.GetDirectories(targetDirectory);
		foreach (string subdirectory in subdirectoryEntries)
			AddDependencyProcessDirectoryRecursive(subdirectory, addToList);
	}

	public void AddDependencyProcessFile(string path, IList<string> addToList)
	{
		addToList.Add(path);
	}

	public EvercoastPlayback(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Projects",
			"InputCore",
			"HeadMountedDisplay",
			"AudioMixer",
			"SignalProcessing",
			"HTTP",
			"Niagara"
		});

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"GhostTree",
				"Projects"
			}
		);

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
			"RHI",
			"ProceduralMeshComponent",
			"MediaAssets",
			"MediaUtils",
			"MovieScene",
			"MovieSceneTracks",
			"Renderer",			// https://docs.unrealengine.com/5.2/en-US/unreal-engine-5.2-release-notes/
			"AudioExtensions"	// IAudioProxyDataFactory which USoundWave derived from
		});

		PublicIncludePaths.AddRange(
			new string[] {
				// ...
			}
			);

		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(EngineDirectory, "Shaders", "Shared"),
				Path.Combine(ModuleDirectory, "Private", "ECV"),
			});

		// Ghost Tree
		PublicIncludePaths.Add(Path.Combine(ThirdPartyRoot, "GhostTree", "include"));
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Ghost Tree
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "Windows", "reading.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "Windows", "decoding.lib"));
			// copy dll to binaries output folder
			RuntimeDependencies.Add("$(BinaryOutputDir)/reading.dll", Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "Windows", "reading.dll"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/decoding.dll", Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "Windows", "decoding.dll"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "Linux", "libreading.so"));
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "Linux", "libdecoding.so"));
			// copy .so to binaries output folder
			RuntimeDependencies.Add("$(BinaryOutputDir)/libreading.so", Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "Linux", "libreading.so"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/libdecoding.so", Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "Linux", "libdecoding.so"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "Mac", "libreading.dylib"));
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "Mac", "libdecoding.dylib"));
			// copy .so to binaries output folder
			RuntimeDependencies.Add("$(BinaryOutputDir)/libreading.dylib", Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "Mac", "libreading.dylib"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/libdecoding.dylib", Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "Mac", "libdecoding.dylib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "Android", "arm64-v8a", "libreading.so"));
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "Android", "arm64-v8a", "libdecoding.so"));
			
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "Android", "armeabi-v7a", "libreading.so"));
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "Android", "armeabi-v7a", "libdecoding.so"));
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicAdditionalFrameworks.Add(new Framework("decoding", Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "iOS", "decoding.framework"), "", true));
			PublicAdditionalFrameworks.Add(new Framework("reading", Path.Combine(ThirdPartyRoot, "GhostTree", "lib", "iOS", "reading.framework"), "", true));
		}
		else
        {
			// Not supported platform, fail the build
			throw new EvercoastUnsupportedPlatformException();
		}
  
        // Corto
        PublicIncludePaths.Add(Path.Combine(ThirdPartyRoot, "corto", "include"));
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "corto", "lib", "Windows", "corto_dll.lib"));
            RuntimeDependencies.Add("$(BinaryOutputDir)/corto_dll.dll", Path.Combine(ThirdPartyRoot, "corto", "lib", "Windows", "corto_dll.dll"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            // Well we copied Kieran's build here so have to stick with the naming :)
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "corto", "lib", "Android", "arm64-v8a", "libcorto_dll.so"));
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "corto", "lib", "Android", "armeabi-v7a", "libcorto_dll.so"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "corto", "lib", "Mac", "libcorto_dll.dylib"));
            // copy .dylib to binaries output folder
            RuntimeDependencies.Add("$(BinaryOutputDir)/libcorto_dll.dylib", Path.Combine(ThirdPartyRoot, "corto", "lib", "Mac", "libcorto_dll.dylib"));
        }
        else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PublicAdditionalFrameworks.Add(new Framework("corto_dll", Path.Combine(ThirdPartyRoot, "corto", "lib", "iOS", "corto_dll.framework"), "", true));
        }
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			// It seems only static lib won't cause libc++ std::string ABI incompabitibility at the moment
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "corto", "lib", "Linux", "libcorto.a"));
            //RuntimeDependencies.Add("$(BinaryOutputDir)/libcorto_dll.so", Path.Combine(ThirdPartyRoot, "corto", "lib", "Linux", "libcorto_dll.so"));
		}
        else
        {
            // Not supported platform, fail the build
            throw new EvercoastUnsupportedPlatformException();
        }

		// libwebp
		PublicIncludePaths.Add(Path.Combine(ThirdPartyRoot, "libwebp", "include"));
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "libwebp", "lib", "Windows", "libwebp.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			// We built libwebp ourselves from: https://github.com/webmproject/libwebp
			// cmake -GNinja -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 -Bbuild -DCMAKE_BUILD_TYPE=Release .
			// cmake --build build --config Release
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "libwebp", "lib", "Mac", "libwebp.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// We built libwebp ourselves from: https://github.com/webmproject/libwebp
			// $> ndk-build NDK_PROJECT_PATH=. APP_PLATFORM=android-21 APP_BUILD_SCRIPT=Android.mk ENABLE_SHARED=1
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "libwebp", "lib", "Android", "arm64-v8a", "libwebpdecoder.so"));
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "libwebp", "lib", "Android", "armeabi-v7a", "libwebpdecoder.so"));
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicAdditionalFrameworks.Add(new Framework("WebP", Path.Combine(ThirdPartyRoot, "libwebp", "lib", "iOS", "WebP.framework"), "", false));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "libwebp", "lib", "Linux", "libwebp.a"));
		}
		else
		{
			// Not supported platform, fail the build
			throw new EvercoastUnsupportedPlatformException();
		}

		// dr_libs - header only libs
		PublicIncludePaths.Add(Path.Combine(ThirdPartyRoot, "dr_libs", "include"));
		PublicDefinitions.AddRange(
			new string[]
			{
				"DR_WAV_IMPLEMENTATION=1",
				"DR_MP3_IMPLEMENTATION=1"
			}
		);

		// intrin compatibility - header only lib for Linux
		if (Target.Platform == UnrealTargetPlatform.Linux)
        {
			PublicIncludePaths.Add(Path.Combine(ThirdPartyRoot, "intrin_compatibility", "include"));
		}


		PublicIncludePaths.Add(Path.Combine(ThirdPartyRoot, "ffmpeg", "include"));
		// FFmpeg, only on Linux, Windows, Android, macOS and iOS
		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.AddRange(new string[]{
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Linux", "x86_64", "libavcodec.so"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Linux", "x86_64", "libavformat.so"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Linux", "x86_64", "libavdevice.so"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Linux", "x86_64", "libavfilter.so"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Linux", "x86_64", "libavutil.so"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Linux", "x86_64", "libswscale.so"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Linux", "x86_64", "libswresample.so")
			});
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.AddRange(new string[]{
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Windows", "x86_64", "avcodec.lib"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Windows", "x86_64", "avformat.lib"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Windows", "x86_64", "avdevice.lib"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Windows", "x86_64", "avfilter.lib"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Windows", "x86_64", "avutil.lib"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Windows", "x86_64", "swscale.lib"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Windows", "x86_64", "swresample.lib")
			});

			RuntimeDependencies.Add("$(BinaryOutputDir)/avcodec-59.dll", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Windows", "x86_64", "avcodec-59.dll"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/avdevice-59.dll", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Windows", "x86_64", "avdevice-59.dll"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/avfilter-8.dll", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Windows", "x86_64", "avfilter-8.dll"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/avformat-59.dll", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Windows", "x86_64", "avformat-59.dll"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/avutil-57.dll", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Windows", "x86_64", "avutil-57.dll"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/swresample-4.dll", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Windows", "x86_64", "swresample-4.dll"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/swscale-6.dll", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Windows", "x86_64", "swscale-6.dll"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// From UE 5.2 Target.Architecture is of type UnrealArch
			string supportedArchitecture = Target.Architecture.ToString();
			if (string.IsNullOrEmpty(supportedArchitecture))
			{
				supportedArchitecture = "arm64-v8a";
			}
			// UE 5.3 changes the name...
			if (supportedArchitecture.ToLower().StartsWith("arm64"))
			{
				supportedArchitecture = "arm64-v8a";
			}
			// Looks like only armv8 is supported. x86_64 requires Unreal source build, armv7 doesn't bother
			PublicAdditionalLibraries.AddRange(new string[]{
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Android", supportedArchitecture, "libavcodec.so"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Android", supportedArchitecture, "libavformat.so"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Android", supportedArchitecture, "libavdevice.so"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Android", supportedArchitecture, "libavfilter.so"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Android", supportedArchitecture, "libavutil.so"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Android", supportedArchitecture, "libswscale.so"),
				Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Android", supportedArchitecture, "libswresample.so")
				// Avoid libc++_shared.so clash, ffmpeg was built against r22b and UE4.27 was against r21b, only having 
				// break change when r23 is introduced. Mind that for future UE releases.
				//Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Android", supportedArchitecture, "libc++_shared.so")
			});
		}
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
        	// There's no clean way of loading frameworks on Mac: https://forums.unrealengine.com/t/how-can-i-integrate-a-third-party-framework-into-my-unreal-project-in-xcode/295511/2
        	// So build dylib from ffmpeg directly
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Mac", "libavcodec.dylib"));
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Mac", "libavdevice.dylib"));
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Mac", "libavfilter.dylib"));
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Mac", "libavformat.dylib"));
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Mac", "libavutil.dylib"));
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Mac", "libswresample.dylib"));
            PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Mac", "libswscale.dylib"));

            // copy .so to binaries output folder
			RuntimeDependencies.Add("$(BinaryOutputDir)/libavcodec.dylib", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Mac", "libavcodec.dylib"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/libavdevice.dylib", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Mac", "libavdevice.dylib"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/libavfilter.dylib", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Mac", "libavfilter.dylib"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/libavformat.dylib", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Mac", "libavformat.dylib"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/libavutil.dylib", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Mac", "libavutil.dylib"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/libswresample.dylib", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Mac", "libswresample.dylib"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/libswscale.dylib", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "Mac", "libswscale.dylib"));
        }
        else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
        	PublicAdditionalFrameworks.Add(new Framework("libavcodec", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "iOS", "libavcodec.framework"), "", true));
        	PublicAdditionalFrameworks.Add(new Framework("libavdevice", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "iOS", "libavdevice.framework"), "", true));
        	PublicAdditionalFrameworks.Add(new Framework("libavfilter", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "iOS", "libavfilter.framework"), "", true));
        	PublicAdditionalFrameworks.Add(new Framework("libavformat", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "iOS", "libavformat.framework"), "", true));
        	PublicAdditionalFrameworks.Add(new Framework("libavutil", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "iOS", "libavutil.framework"), "", true));
        	PublicAdditionalFrameworks.Add(new Framework("libswresample", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "iOS", "libswresample.framework"), "", true));
        	PublicAdditionalFrameworks.Add(new Framework("libswscale", Path.Combine(ThirdPartyRoot, "ffmpeg", "lib", "iOS", "libswscale.framework"), "", true));
        }

        // PicoQuic
		PublicIncludePaths.Add(Path.Combine(ThirdPartyRoot, "picoquic", "include"));
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			RuntimeDependencies.Add("$(BinaryOutputDir)/picoquicclient.dll", Path.Combine(ThirdPartyRoot, "picoquic", "lib", "Windows", "picoquicclient.dll"));
            RuntimeDependencies.Add("$(BinaryOutputDir)/picoquicclient_old.dll", Path.Combine(ThirdPartyRoot, "picoquic", "lib", "Windows", "picoquicclient_old.dll"));
        }
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "picoquic", "lib", "Android", "libpicoquicclient.so"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "picoquic", "lib", "Linux", "libpicoquicclient.so"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "picoquic", "lib", "Mac", "libpicoquicclient.dylib"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/libpicoquicclient.dylib", Path.Combine(ThirdPartyRoot, "picoquic", "lib", "Mac", "libpicoquicclient.dylib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "picoquic", "lib", "iOS", "libpicoquicclient.a"));

			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "picoquic", "lib", "iOS", "libcrypto.a"));
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "picoquic", "lib", "iOS", "libpicoquic-core.a"));
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "picoquic", "lib", "iOS", "libpicoquic-log.a"));
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "picoquic", "lib", "iOS", "libpicotls-core.a"));
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "picoquic", "lib", "iOS", "libpicotls-openssl.a"));
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "picoquic", "lib", "iOS", "libssl.a"));
		}
		else
		{
			// Not supported platform, fail the build
			throw new EvercoastUnsupportedPlatformException();
		}

		// Zstd
		PublicIncludePaths.Add(Path.Combine(ThirdPartyRoot, "zstd", "include"));
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyRoot, "zstd", "dll", "libzstd.dll.a"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/libzstd.dll", Path.Combine(ThirdPartyRoot, "zstd", "dll", "libzstd.dll"));
		}

		if (Target.Platform == UnrealTargetPlatform.Android)
        {
			string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "EvercoastPlayback_APL.xml"));
		}

		string cookedDataPath = Path.Combine(ProjectRoot, "Content", "EvercoastVolcapSource");
		if (!Directory.Exists(cookedDataPath))
		{
			Directory.CreateDirectory(cookedDataPath);
		}

		if (ReadOnlyBuildVersion.Current.MajorVersion == 5 && ReadOnlyBuildVersion.Current.MinorVersion >= 3)
		{
			CppStandard = CppStandardVersion.Latest;
		}
		else
		{
			CppStandard = CppStandardVersion.Cpp17;
		}
	}
}
