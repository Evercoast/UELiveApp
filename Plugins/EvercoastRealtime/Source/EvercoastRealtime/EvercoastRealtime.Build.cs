/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
*/

using System;
using System.IO;
using UnrealBuildTool;
using System.Collections.Generic;

public class EvercoastRealtime : ModuleRules
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
	
	public EvercoastRealtime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Projects",
				"EvercoastPlayback"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// ... add private dependencies that you statically link with here ...	
				"AudioExtensions",
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

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

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "EvercoastRealtime_APL.xml"));
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
