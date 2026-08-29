#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBInputActionAbility.h"
#include "CBChaserAttackAbility.generated.h"

/**
 * 추격자 공격 어빌리티.
 * 콤보 여부 및 공격력 계수를 설정할 수 있으며, 공격 시 타겟에게 데미지 GE를 적용한다.
 */
UCLASS()
class CHAINBURST_API UCBChaserAttackAbility : public UCBInputActionAbility
{
	GENERATED_BODY()

public:
	UCBChaserAttackAbility();

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

	//~ Begin UCBActionAbility Interface
	/** 콤보 액션이면 콤보 인덱스를 전진시켜 반환, 아니면 0 */
	virtual int32 SelectActionMontageIndex() override;
	/** 액션 종료(정상·폴백·캔슬) 시 콤보 리셋 */
	virtual void CleanupActionState() override;
	//~ End UCBActionAbility Interface

	/** 이 어빌리티와 연결된 액션(몽타주)의 콤보 여부. 끄면 단일 몽타주(인덱스 0)로 재생 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	bool IsCombo = false;

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
