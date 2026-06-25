#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBChaserActionAbility.h"
#include "CBGAChaserEquipWeapon.generated.h"

class UAbilityTask_WaitGameplayEvent;

UCLASS()
class CHAINBURST_API UCBGAChaserEquipWeapon : public UCBChaserActionAbility
{
	GENERATED_BODY()

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

	/** 입력 확인 시 호출되는 함수 (태그 이벤트 대기) */
	UFUNCTION()
	void OnEquipEvent(FGameplayEventData Payload);
	
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> EquipEventTask;
};
