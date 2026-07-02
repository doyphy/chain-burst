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
 * 모든 액션 어빌리티는 몽타주를 UCBActionMontageData에서 가져오므로,
 * 싱글/콤보 몽타주 구분에 쓰이는 IsCombo는 이 베이스에서 공통으로 관리.
 *
 * 자식 확장 지점:
 *   - BuildActionCueParameters()   : 큐 파라미터에 추가 태그를 붙임 (예: 피격 방향 등)
 *   - OnActionMontageStarted()     : 몽타주 재생 직후 후처리 (예: 입력 대기 태스크 등록)
 *   - ShouldResetComboOnEnd()      : 액션 종료 시 콤보 인덱스 초기화 여부 (기본: IsCombo)
 */
UCLASS(Abstract)
class CHAINBURST_API UCBActionAbility : public UCBGameplayAbility
{
	GENERATED_BODY()

protected:
	/** 액션 몽타주 재생 및 종료 처리 등록 (자식에서 호출) */
	void PlayActionMontage();

	/**
	 * 몽타주 재생용 GameplayCue 파라미터 구성 훅 (자식이 추가 태그를 붙임)
	 * 콤보 여부는 베이스에서 처리하므로, 그 외 태그가 필요할 때 재정의.
	 */
	virtual void BuildActionCueParameters(FGameplayCueParameters& CueParams) {}

	/** 몽타주 재생 시작 직후 후처리 훅 (자식이 추가 작업을 수행) */
	virtual void OnActionMontageStarted() {}

	/** 액션 종료 시 콤보 인덱스를 초기화할지 여부 (기본: 콤보 액션이면 초기화) */
	virtual bool ShouldResetComboOnEnd() const { return IsCombo; }

	/** 현재 재생 중인 액션의 길이 반환 (폴백 딜레이용) */
	float CurrentActionDuration() const;

	/** 액션 종료 이벤트(애님노티파이) 수신 시 호출 */
	UFUNCTION()
	virtual void OnActionEnded(FGameplayEventData Payload);

	/** 폴백 타임아웃 시 호출 (애님노티파이가 없는 경우) */
	UFUNCTION()
	virtual void OnDelayFinished();

protected:
	/** 이 어빌리티와 연결된 액션(몽타주) 태그 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst", meta = (Categories = "Action"))
	FGameplayTag BoundActionTag;

	/** 이 어빌리티와 연결된 액션(몽타주)의 콤보 여부 (싱글/콤보 몽타주 요청에 사용) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst")
	bool IsCombo = false;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> EndActionTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> DelayTask;
};
