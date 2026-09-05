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

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "EngineSettings", "MotionWarping", "NavigationSystem", "OnlineServicesCommon" });

		PrivateDependencyModuleNames.AddRange(new string[] { "CoreOnline", "OnlineServicesInterface", "Sockets"});
		
		// EOS Device ID 로그인용 (SDK 직접 호출)
		PrivateDependencyModuleNames.AddRange(new string[] { "EOSShared", "EOSSDK" });
	}
}
