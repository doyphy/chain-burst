#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBChaserGameplayAbility.h"
#include "CBChaserActionAbility.generated.h"

class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitDelay;

/**
 * 체이서의 액션(몽타주) 어빌리티
 * - BoundInputTag에 매핑된 입력이 감지되면 어빌리티 재활성화 시도
 * - BoundActionGameplayCueTag에 매핑된 게임플레이 큐 실행 (몽타주 재생)
 * - 입력 이벤트를 받으면 입력 감지 후 어빌리티 재활성화 (어빌리티 연속 실행)
 * - 어빌리티 종료 이벤트를 받으면 어빌리티 종료 및 콤보 초기화 (몽타주 끊고 다른 어빌리티가 실행될 수 있게)
 * - 몽타주가 완전히 끝나면 자동으로 어빌리티 종료 및 콤보 초기화
 */

UCLASS()
class CHAINBURST_API UCBChaserActionAbility : public UCBChaserGameplayAbility
{
	GENERATED_BODY()
protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	//~ End UGameplayAbility Interface

	/** 액션 몽타주 재생 함수 (자식에서 호출) */
	void PlayActionMontage();
	
	/** 현재 재생중인 액션의 재생 시간 반환 */
	float CurrentActionDuration() const;
	
	/** 이 어빌리티와 연결된 입력 태그 (입력 감지) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst", meta = (Categories = "Input"))
	FGameplayTag BoundInputTag;

	/** 이 어빌리티와 연결된 액션(몽타주) 태그 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst", meta = (Categories = "Action"))
	FGameplayTag BoundActionTag;

	/** 이 어빌리티와 연결된 액션(몽타주)의 콤보 여부 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst")
	bool IsCombo;

	/** 몽타주 재생 가능 여부 */
	bool bCanPlayMontage = false;
private:
	/** 현재 입력 홀드 여부 */
	bool bIsInputHeld = false;

	/** 입력 대기 중 여부 (CheckInput 윈도우가 열린 상태) */
	bool bWaitingForInput = false;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> CheckInputTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> EndActionTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> DelayTask;

	/** 액션 종료 시 호출되는 함수 (태그 이벤트 대기) */
	UFUNCTION()
	void OnActionEnded(FGameplayEventData Payload);

	/** 입력 확인 시 호출되는 함수 (태그 이벤트 대기) */
	UFUNCTION()
	void OnCheckInput(FGameplayEventData Payload);

	/** 타임아웃 시 호출되는 함수 */
	UFUNCTION()
	void OnDelayFinished();
};
