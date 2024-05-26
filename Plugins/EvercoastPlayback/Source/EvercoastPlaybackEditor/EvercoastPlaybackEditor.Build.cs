/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-17 04:06:53
* @Last Modified by:   feng_ye
* @Last Modified time: 2023-10-26 21:14:02
*/
using System;
using System.IO;
using UnrealBuildTool;

public class EvercoastPlaybackEditor : ModuleRules
{
	public EvercoastPlaybackEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		RuntimeDependencies.Add("AssetTools");

		PublicDependencyModuleNames.AddRange(new string[]
		{
		});


		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"ContentBrowser",
			"DesktopWidgets",
			"DesktopPlatform",
			"EditorStyle",
			"Projects",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"ToolMenus",
			"EvercoastPlayback",
			"MovieScene",
			"MovieSceneTracks",
			"MovieSceneTools",
			"Sequencer",
			"TimeManagement",
			"LevelSequence",
			"LevelEditor"
		});


		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"UnrealEd",
				"ToolMenus"
			});

		if (ReadOnlyBuildVersion.Current.MajorVersion == 5)
		{
			PrivateDependencyModuleNames.Add("DeveloperToolSettings");
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
