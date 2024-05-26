/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-18 22:36:37
* @Last Modified by:   Ye Feng
* @Last Modified time: 2021-11-18 22:40:06
*/

using System.IO;
using UnrealBuildTool;

public class GhostTree : ModuleRules
{
	public GhostTree(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		CppStandard = CppStandardVersion.Cpp17;
	}
}
