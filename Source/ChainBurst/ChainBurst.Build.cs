// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChainBurst : ModuleRules
{
	public ChainBurst(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"GameplayTags", "GameplayAbilities", "GameplayTasks", "UMG", "AIModule"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "EngineSettings", "MotionWarping" });

		PrivateDependencyModuleNames.AddRange(new string[] { "CoreOnline", "OnlineServicesInterface", "Sockets" });
	}
}
