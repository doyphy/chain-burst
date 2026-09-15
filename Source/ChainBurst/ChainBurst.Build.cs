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

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "EngineSettings", "MotionWarping", "NavigationSystem" });

		PrivateDependencyModuleNames.AddRange(new string[] { "CoreOnline", "OnlineServicesInterface", "Sockets" });

		// Device ID 생성(EOS_Connect_CreateDeviceId)을 OSSv2 가 감싸주지 않아 SDK 를 직접 호출함.
		// 이 의존성을 쓰는 코드는 CBAuthSubsystem.cpp 하나로 제한할 것
		PrivateDependencyModuleNames.AddRange(new string[] { "EOSShared", "EOSSDK" });
	}
}
