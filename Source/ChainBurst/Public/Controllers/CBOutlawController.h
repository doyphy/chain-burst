#pragma once

#include "CoreMinimal.h"
#include "Controllers/CBAIController.h"
#include "CBOutlawController.generated.h"

/**
 * [AI] Outlaw(보스/엘리트) 전용 컨트롤러.
 * 베이스와 같이 로드아웃의 비헤이비어 트리로 구동함.
 * 보스는 한 번 찍은 타겟을 인지를 잃어도 놓지 않음 - 숨어도 위치를 알고 계속 추격·주시.
 * (최초 획득은 베이스와 같이 퍼셉션)
 *
 * 교체는 점수 배수 비교 대신 "집중" 규칙을 따름 (SwitchScoreRatio 는 쓰지 않음):
 * - 집중 유지 : 잡은 지 MaxFocusTime 미만 + (잡은 지 RecentDamageMemoryTime 미만(유예) 또는 현재 타겟이 최근에 나를 때림)
 * - 조용해서 끝남 (도망·회피) : 현재 타겟도 후보에 넣어 점수로 다시 고름 (때린 사람 > 가까운 사람)
 * - 최대 시간으로 끝남 : 현재 타겟을 빼고 다시 고름 (순환). 다른 후보가 없으면 같은 대상으로 다시 집중
 * 공격 중에는 교체하지 않음 (베이스의 전환 금지 구간).
 */
UCLASS()
class CHAINBURST_API ACBOutlawController : public ACBAIController
{
	GENERATED_BODY()

protected:
	//~ Begin ACBAIController Interface
	/** 인지 여부와 무관하게 유지 (적 판정 + 생존만). 사망·파괴·진영 변화일 때만 해제. */
	virtual bool IsTargetStillValid(AActor* InActor) const override;

	/** 집중이 끝났을 때만 다시 고름 (끝난 이유에 따라 현재 타겟을 후보에 넣거나 뺌). */
	virtual void ReevaluateTarget(AActor* InCurrentTarget) override;
	//~ End ACBAIController Interface

	/**
	 * 한 대상에게 집중하는 최대 시간(초). 지나면 다른 대상으로 순환 (한 사람만 계속 맞고 나머지가 쉬는 것을 막음).
	 * 0 이하면 순환하지 않음 - 현재 타겟이 조용해질 때만 교체.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|AI|Targeting", meta = (ClampMin = "0.0", Units = "s"))
	float MaxFocusTime = 12.f;
};
