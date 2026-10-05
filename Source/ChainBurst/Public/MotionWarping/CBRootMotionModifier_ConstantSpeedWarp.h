#pragma once

#include "CoreMinimal.h"
#include "RootMotionModifier.h"
#include "CBRootMotionModifier_ConstantSpeedWarp.generated.h"

/**
 * 워프 타겟을 향해 일정 속도로 직진하는 워프 모디파이어 (돌진·전진 공격용).
 *
 * 엔진 Skew Warp 는 "구간이 끝날 때 타겟에 도착"하도록 매 프레임 속도를 정하므로 타겟이 멀수록 빨라짐.
 * 이 모디파이어는 속도를 고정하고, 이동 거리가 결과가 되도록 뒤집음.
 * - 방향 : 구간이 활성화되는 순간의 워프 타겟 위치를 향해 고정. 이후 타겟이 움직여도 직진 (옆으로 피할 수 있음)
 * - 이동 : 매 프레임 Speed × 시간만큼 수평 이동. 애니메이션 자체 이동은 쓰지 않음 (제자리 클립 전제)
 * - 정지 : 고정한 도착 지점에 닿거나, 이동이 막히면(요청 대비 실제 이동이 크게 모자람) 구간이 남아도 멈춤
 * - 섹션 : 멈춘 순간 StopSectionName 이 있으면 [서버] 그 섹션으로 점프 요청 (Event.Action.JumpSection → 어빌리티가 큐로 전 머신에 전달)
 * - 회전 : 고정한 방향을 바라봄. WarpMaxRotationRate > 0 이면 그 속도(도/초)로, 0 이면 즉시
 * 도착 지점은 워프 타겟 위치 그대로라, 타겟 앞 정지 거리는 워프 타겟을 등록하는 쪽(AI 공격의 WarpStopDistance)이 정함.
 *
 * 속도·회전 제한은 몽타주 재생 속도(PlayRate)와 무관한 실제 시간 기준 (돌진은 공격 속도로 빨라지지 않음).
 * 공격 속도 적용 액션(몽타주 데이터의 bAffectedByAttackSpeed)에는 쓰지 말 것.
 * - 배속이 붙으면 구간만 짧아져, 버프를 받을 때만 도달 거리가 조용히 줄어듦.
 */
UCLASS(meta = (DisplayName = "Constant Speed Warp"))
class CHAINBURST_API UCBRootMotionModifier_ConstantSpeedWarp : public URootMotionModifier_Warp
{
	GENERATED_BODY()

public:
	//~ Begin URootMotionModifier Interface
	/** 매 프레임 호출되어 루트모션을 일정 속도 이동으로 바꿈 */
	virtual FTransform ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds) override;
	/** 워프 상태가 바뀔 때 호출됨. 활성화될 때마다 방향을 다시 고정하도록 초기화 */
	virtual void OnStateChanged(ERootMotionModifierState LastState) override;
	//~ End URootMotionModifier Interface

protected:
	/** 이동 속도 (실제 시간 cm/초). 타겟까지의 거리·몽타주 재생 속도와 무관하게 이 속도로 직진 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "1.0"))
	float Speed = 800.f;

	/**
	 * 도착·막힘으로 멈췄을 때 넘어갈 몽타주 섹션. None 이면 이동만 멈추고 몽타주는 그대로 진행.
	 * 이 워프 구간보다 뒤에 있는 섹션이어야 함 (이미 그 섹션 시작을 지난 머신은 되감지 않고 무시).
	 * 판정은 서버 권위라, 플레이어 몽타주에 쓰면 소유 클라는 지연만큼 늦게 점프함.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FName StopSectionName = NAME_None;

private:
	/** [서버] StopSectionName 으로 점프 요청. 이동 처리 도중에 불리므로 이벤트는 다음 틱에 보냄 */
	void RequestStopSectionJump() const;

	/** 이동 방향 (수평, 활성화 후 첫 프레임에 고정) */
	FVector MoveDirection = FVector::ZeroVector;

	/** 고정 시점의 위치 (이동한 거리의 기준점) */
	FVector MoveStartLocation = FVector::ZeroVector;

	/** 고정 시점에 잰 도착 지점까지의 수평 거리 */
	float TravelDistance = 0.f;

	/** 지난 프레임 위치 (막힘 판정용) */
	FVector LastLocation = FVector::ZeroVector;

	/** 지난 프레임에 요청한 이동 거리 (막힘 판정용, 0 이면 판정 생략) */
	float LastRequestedDistance = 0.f;

	/** 방향·거리를 고정했는지 여부 */
	bool bDirectionLocked = false;

	/** 도착·막힘으로 이동을 멈췄는지 여부 (구간이 끝날 때까지 유지) */
	bool bMoveStopped = false;

	/** 이번 구간에서 섹션 점프를 이미 요청했는지 여부 (멈춘 순간 1회만) */
	bool bStopSectionRequested = false;
};
