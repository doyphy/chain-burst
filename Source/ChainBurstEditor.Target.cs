// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ChainBurstEditorTarget : TargetRules
{
	public ChainBurstEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		// UE5.7: V6 부터 UndefinedIdentifierWarningLevel 기본값이 Error. 설치형 엔진(UnrealEditor)이
		// V6로 컴파일돼 있어, 공유 빌드환경 검증을 통과하려면 게임 타겟도 V6를 따라야 한다.
		// IncludeOrder는 5.8 도착 후 일괄 상향 예정이라 아직 Unreal5_5 유지.
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("ChainBurst");
	}
}
