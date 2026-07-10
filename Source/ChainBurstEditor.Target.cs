// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ChainBurstEditorTarget : TargetRules
{
	public ChainBurstEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		// UE5.8: V7 이 Return/Dangling/UnreachableCode WarningLevel 기본값을 Error 로 바꿈. 설치형
		// UnrealEditor 가 V7로 컴파일돼 있어 공유 빌드환경 검증을 통과하려면 게임 타겟도 V7 필요.
		// (MSVC 기준 이 셋은 이미 Error 였으므로 실제 컴파일 동작 변화는 없음)
		// IncludeOrder: 5.8 Oldest 가 Unreal5_6 이라 최신 Unreal5_8 로 상향.
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("ChainBurst");
	}
}
