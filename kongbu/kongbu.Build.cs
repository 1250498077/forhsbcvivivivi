// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class kongbu : ModuleRules
{
	public kongbu(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		MinFilesUsingPrecompiledHeaderOverride = 1;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AIModule", "NavigationSystem", "OnlineSubsystem", "OnlineSubsystemUtils", "AssetRegistry", "PhysicsCore"  });

		PrivateDependencyModuleNames.AddRange(new string[] {  });
		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
