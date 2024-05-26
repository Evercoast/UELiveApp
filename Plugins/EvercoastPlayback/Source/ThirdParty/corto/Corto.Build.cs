/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-18 22:36:37
* @Last Modified by:   feng_ye
* @Last Modified time: 2024-02-15 12:29:46
*/

using System.IO;
using UnrealBuildTool;

public class Corto : ModuleRules
{
	public Corto(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		CppStandard = CppStandardVersion.Cpp17;
	}
}
