#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBInputActionAbility.h"
#include "CBChaserAttackAbility.generated.h"

/**
 * 추격자 공격 어빌리티.
 * 콤보 여부 및 공격력 계수를 설정할 수 있으며, 공격 시 타겟에게 데미지 GE를 적용함.
 * bAlignToAimOnActivate - 발동 시 캐릭터를 조준 방향으로 정렬해 몽타주가 카메라가 보는 쪽으로 재생.
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

	/**
	 * 발동 시 캐릭터를 조준(컨트롤 회전) 방향으로 즉시 정렬할지 여부 (기본값 : true).
	 * 끄면 발동 시점의 몸 방향 그대로 몽타주가 재생.
	 * 콤보는 매 타격마다 어빌리티를 재활성화하므로, 켜두면 타격마다 현재 조준으로 다시 정렬됨.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	bool bAlignToAimOnActivate = true;

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
