#pragma once

#include "CoreMinimal.h"
#include "RootMotionModifier_SkewWarp.h"
#include "CBRootMotionModifier_ClampedSkewWarp.generated.h"

/**
 * 워프 이동 거리에 상한을 둔 Skew Warp 모디파이어.
 *
 * 엔진 SkewWarp의 MaxSpeedClampRatio 는 클립에 루트모션 이동이 있을 때만 적용됨.
 * 제자리 클립은 "이동이 없으면 만들어 넣는" 경로를 타는데 거기엔 상한이 없어서,
 * 추종 워프 중 타겟이 순간이동하면(대시 등) 그 거리만큼 그대로 끌려감.
 *
 * 그래서 워프 시작 지점에서 MaxWarpDistance를 넘어서는 이동량을 잘라냄.
 */
UCLASS(meta = (DisplayName = "Clamped Skew Warp"))
class CHAINBURST_API UCBRootMotionModifier_ClampedSkewWarp : public URootMotionModifier_SkewWarp
{
	GENERATED_BODY()

public:
	//~ Begin URootMotionModifier Interface
	/** 매 프레임 호출되어 루트모션을 처리함 */
	virtual FTransform ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds) override;
	/** 워프 상태가 바뀔 때 호출됨 */
	virtual void OnStateChanged(ERootMotionModifierState LastState) override;
	//~ End URootMotionModifier Interface

protected:
	/** 워프 시작 지점에서 이동할 수 있는 최대 거리(cm). 0 이하면 제한 없음(엔진 기본 동작). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "0.0"))
	float MaxWarpDistance = 200.f;

private:
	/** 거리 제한의 기준점 (워프 활성화 시점의 캐릭터 위치). */
	FVector ClampOriginLocation = FVector::ZeroVector;

	/** ClampOriginLocation 이 채워졌는지 여부 (활성화 전에는 베이스 값으로 폴백) */
	bool bHasClampOrigin = false;
};
