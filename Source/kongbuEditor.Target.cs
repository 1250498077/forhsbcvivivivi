// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class kongbuEditorTarget : TargetRules
{
	public kongbuEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		bUseUnityBuild = true;
		bUseAdaptiveUnityBuild = false;
		bUsePCHFiles = true;
		ExtraModuleNames.Add("kongbu");
	}
}
