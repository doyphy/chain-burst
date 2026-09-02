#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBGameplayAbility.h"
#include "CBActionAbility.generated.h"

class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitDelay;

/**
 * 액션(몽타주) 어빌리티의 공용 베이스 (추상 클래스)
 * - GameplayCue.PlayAction을 통해 몽타주를 재생 (전 클라 동기화)
 * - 애님노티파이 종료 이벤트(Event.Action.EndAbility) 또는 폴백 딜레이로 어빌리티 종료
 * - 트리거 방식(입력 / 이벤트)은 자식 클래스에서 결정
 *
 * 자식 확장 지점:
 *   - SelectActionMontageIndex()   : 재생 인덱스 결정 (기본 0, 예: 대시의 전투/비전투 분기, 공격의 콤보 전진)
 *   - BuildActionCueParameters()   : 큐 파라미터에 추가 데이터를 실음 (예: 피격 방향, 모션 워핑 타겟 위치 등)
 *   - OnActionMontageStarted()     : 몽타주 재생 직후 후처리 (예: 입력 대기 태스크 등록)
 *   - CleanupActionState()         : 액션이 끝났을 때 자식 상태 정리 (예: 콤보 리셋)
 */
UCLASS(Abstract)
class CHAINBURST_API UCBActionAbility : public UCBGameplayAbility
{
	GENERATED_BODY()

protected:
	//~ Begin UGameplayAbility Interface
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

	/** 액션 몽타주 재생 및 종료 처리 등록 (자식에서 호출) */
	void PlayActionMontage();

	/**
	 * 재생 인덱스 결정 훅 (기본 0)
	 * 자식이 상태에 따라 변형 몽타주를 고를 때 재정의한다.
	 * const가 아닌 이유: 콤보처럼 인덱스를 정하면서 상태를 전진시키는 구현을 허용하기 위함.
	 */
	virtual int32 SelectActionMontageIndex() { return 0; }

	/** 몽타주 재생용 GameplayCue 파라미터 구성 훅 (자식이 추가 태그를 붙임) */
	virtual void BuildActionCueParameters(FGameplayCueParameters& CueParams) {}

	/** 몽타주 재생 시작 직후 후처리 훅 (자식이 추가 작업을 수행) */
	virtual void OnActionMontageStarted() {}

	/**
	 * 액션이 끝났을 때 자식이 자기 상태를 정리하는 훅.
	 * 정상 종료(노티파이)·폴백 타임아웃·캔슬 세 경로 모두에서 호출된다.
	 */
	virtual void CleanupActionState() {}

	/**
	 * 액션 종료 시 몽타주를 정지시킬지 여부 (기본 true).
	 * 사망처럼 마지막 프레임을 유지해야 하는 액션은 false를 반환한다.
	 * (false를 반환해도 몽타주가 끝에서 자동 블렌드 아웃되지 않으려면 몽타주 에셋의 Enable Auto Blend Out을 꺼야 함)
	 */
	virtual bool ShouldStopActionOnEnd() const { return true; }

	/** 현재 재생 중인 액션의 길이 반환 (폴백 딜레이용) */
	float CurrentActionDuration() const;

	/** 액션 종료 이벤트(애님노티파이) 수신 시 호출. ShouldStopActionOnEnd()가 참일 때만 몽타주를 정지시킴 */
	UFUNCTION()
	virtual void OnActionEnded(FGameplayEventData Payload);

	/** 폴백 타임아웃 시 호출 (애님노티파이가 없는 경우) */
	UFUNCTION()
	virtual void OnDelayFinished();

protected:
	/** 이 어빌리티와 연결된 액션(몽타주) 태그 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst", meta = (Categories = "Action"))
	FGameplayTag BoundActionTag;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> EndActionTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> DelayTask;
};
