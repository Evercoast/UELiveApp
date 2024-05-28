using System.IO;
using UnrealBuildTool;


public class PicoQuicClient : ModuleRules
{
	public PicoQuicClient(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		CppStandard = CppStandardVersion.Cpp14;
	}
}
