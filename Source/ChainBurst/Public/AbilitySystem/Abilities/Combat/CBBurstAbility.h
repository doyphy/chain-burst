#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBInputActionAbility.h"
#include "CBBurstAbility.generated.h"

class UGameplayEffect;

/**
 * 버스트 발동 어빌리티 (입력 액션 어빌리티 기반)
 * - 버스트 게이지(BurstGauge)가 가득 찼을 때만 발동 가능
 * - 버스트 GE 적용(예측) → [서버] 게이지 소모 → 발동 몽타주(Action.Combat.Burst) 재생 순으로 처리.
 *   버스트를 몽타주보다 먼저 확정하므로 몽타주가 끊기거나 재생에 실패해도 버스트는 유지됨.
 * - 공격 중 발동 불가는 공격 어빌리티 BP의 BlockAbilitiesWithTag(Ability.Combat)가 이 어빌리티의 태그(Ability.Combat.Burst)를 막아서 처리함.
 * - 공격 속도·공격력 증가는 버스트 GE의 모디파이어가 담당. 기존 소비 경로(몽타주 PlayRate, 데미지 ExecCalc)가 그대로 반영함.
 *
 * 게이지 적립·초기화는 UCBBurstGaugeAbility 담당. 버스트 GE는 BP 자식(GA_Burst)에서 지정.
 */
UCLASS()
class CHAINBURST_API UCBBurstAbility : public UCBInputActionAbility
{
	GENERATED_BODY()

public:
	UCBBurstAbility();

protected:
	//~ Begin UGameplayAbility Interface
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~ End UGameplayAbility Interface

	/** 버스트 효과 GE (지속형, AttackSpeed·AttackPower 배율 + GrantedTags에 Status.Combat.Burst). BP 자식에서 지정 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Burst")
	TSubclassOf<UGameplayEffect> BurstEffectClass;

private:
	/** 버스트 GE 적용(예측) + [서버] 게이지 소모. 몽타주보다 먼저 호출함 */
	void ApplyBurstState(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo);
};
