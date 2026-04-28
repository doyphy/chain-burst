#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBChaserGameplayAbility.h"
#include "CBChaserAttackAbility.generated.h"

UCLASS()
class CHAINBURST_API UCBChaserAttackAbility : public UCBChaserGameplayAbility
{
	GENERATED_BODY()
	
protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

	/** 이 어빌리티와 연결된 입력 태그 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Ability")
	FGameplayTag BoundInputTag;
};
