#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBEventActionAbility.h"
#include "CBDeathAbility.generated.h"

class UGameplayEffect;

/**
 * 사망 어빌리티
 * - Event.Combat.Death 게임플레이 이벤트로 자동 발동 (체력 0 도달 시 서버가 발행)
 * - 진행 중인 다른 어빌리티를 전부 캔슬 → 사망 상태 GE(Status.Dead 부여) 적용 → 사망 몽타주(Action.Combat.Death) 재생 순으로 처리
 * - Status.Dead가 붙으면 UCBGameplayAbility::CanActivateAbility의 공통 게이트가
 *   사망 전용(bActivatableWhileDead) 외의 모든 어빌리티 활성화를 막음.
 *
 * - 사망 몽타주는 종료 시 정지시키지 않아 마지막 프레임이 시체 포즈로 남음.
 *   (몽타주 에셋의 Enable Auto Blend Out을 꺼야 완성됨 — 켜져 있으면 끝에서 저절로 풀림)
 *
 * 사망 GE는 BP 자식(GA_Death)에서 지정.
 */
UCLASS()
class CHAINBURST_API UCBDeathAbility : public UCBEventActionAbility
{
	GENERATED_BODY()

public:
	UCBDeathAbility();

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~ End UGameplayAbility Interface

	//~ Begin UCBActionAbility Interface
	/** 사망 몽타주는 정지시키지 않는다 — 마지막 프레임(시체 포즈)을 유지하기 위함. */
	virtual bool ShouldStopActionOnEnd() const override { return false; }
	//~ End UCBActionAbility Interface

	/** 사망 상태를 부여하는 GE (무한 지속, GrantedTags에 Status.Dead). BP 자식에서 지정 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst")
	TSubclassOf<UGameplayEffect> DeadStateEffectClass;

private:
	/** [서버] 사망 상태 GE를 적용하는 함수 (몽타주보다 먼저 호출 — 재생에 실패해도 사망 상태는 확정) */
	void Auth_ApplyDeadState(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo);
};
