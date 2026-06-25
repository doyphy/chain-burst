#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBChaserActionAbility.h"
#include "CBChaserAttackAbility.generated.h"

UCLASS()
class CHAINBURST_API UCBChaserAttackAbility : public UCBChaserActionAbility
{
	GENERATED_BODY()

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

	/** 타겟에게 적용할 데미지 GE 클래스 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** 데미지 계수 (FinalDamage = AttackPower * DamageCoefficient - DefensePower) */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Damage", meta = (ClampMin = "0.0"))
	float DamageCoefficient = 1.f;

private:
	UFUNCTION()
	void OnTraceStart(FGameplayEventData Payload);

	UFUNCTION()
	void OnTraceEnd(FGameplayEventData Payload);

	UFUNCTION()
	void OnAttackHit(FGameplayEventData Payload);
};
