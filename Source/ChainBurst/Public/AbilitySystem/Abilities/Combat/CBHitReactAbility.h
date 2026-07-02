#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBGameplayAbility.h"
#include "CBHitReactAbility.generated.h"

class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitDelay;

/**
 * 피격 반응 어빌리티
 * - Event.Combat.HitReact 게임플레이 이벤트로 자동 발동 (입력 불필요)
 * - 발동 시 진행 중인 전투 액션(Action.Combat)을 캔슬하고 피격 몽타주 재생
 * - 몽타주 재생은 기존 GameplayCue.PlayAction 파이프라인 재사용 (전 클라 동기화)
 */
UCLASS()
class CHAINBURST_API UCBHitReactAbility : public UCBGameplayAbility
{
	GENERATED_BODY()

public:
	UCBHitReactAbility();

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~ End UGameplayAbility Interface

	/** 재생할 피격 액션(몽타주) 태그 (기본값: Action.Combat.Hit) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst", meta = (Categories = "Action"))
	FGameplayTag HitReactActionTag;

	/** 피격 시 캔슬할 액션 태그 (기본값: Action.Combat) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst", meta = (Categories = "Action"))
	FGameplayTag CancelActionTag;

private:
	/** 피격 몽타주 재생 및 종료 처리 등록 */
	void PlayHitReactMontage();

	/** 몽타주 종료 이벤트 수신 시 호출 (애님노티파이) */
	UFUNCTION()
	void OnHitReactEnded(FGameplayEventData Payload);

	/** 폴백 타임아웃 시 호출 (애님노티파이가 없는 경우) */
	UFUNCTION()
	void OnHitReactTimeout();

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> EndActionTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> DelayTask;
};
