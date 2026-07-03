#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBActionAbility.h"
#include "CBInputActionAbility.generated.h"

class UAbilityTask_WaitGameplayEvent;

/**
 * 입력으로 트리거되는 액션(몽타주) 어빌리티 베이스 (추상 클래스)
 * - BoundInputTag에 매핑된 입력으로 활성화 / 재활성화
 * - 콤보 입력 윈도우 처리 (Event.Action.CheckInput 이벤트 기반)
 * - 주로 플레이어가 능동적으로 사용하는 액션에 사용 (특정 캐릭터에 종속되지 않음)
 *
 * 실제 몽타주 재생(PlayActionMontage)은 조건 검사를 위해 구체 자식 클래스에서 호출.
 */
UCLASS(Abstract)
class CHAINBURST_API UCBInputActionAbility : public UCBActionAbility
{
	GENERATED_BODY()

public:
	UCBInputActionAbility();

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	//~ End UGameplayAbility Interface

	//~ Begin UCBActionAbility Interface
	/**
	 * 몽타주 재생 시작 직후 후처리 훅 (자식이 추가 작업을 수행)
	 * - 로컬 컨트롤 시 입력 확인 윈도우 대기 등록
	 */
	virtual void OnActionMontageStarted() override;
	//~ End UCBActionAbility Interface

	/** 이 어빌리티와 연결된 입력 태그 (입력 감지) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst", meta = (Categories = "Input"))
	FGameplayTag BoundInputTag;

	/** 몽타주 재생 가능 여부 (자식이 조건 검사 후 PlayActionMontage 호출 판단에 사용) */
	bool bCanPlayMontage = false;

private:
	/** 현재 입력 홀드 여부 */
	bool bIsInputHeld = false;

	/** 입력 대기 중 여부 (CheckInput 윈도우가 열린 상태) */
	bool bWaitingForInput = false;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> CheckInputTask;

	/** 입력 확인 시 호출되는 함수 (CheckInput 이벤트 수신) */
	UFUNCTION()
	void OnCheckInput(FGameplayEventData Payload);
};
