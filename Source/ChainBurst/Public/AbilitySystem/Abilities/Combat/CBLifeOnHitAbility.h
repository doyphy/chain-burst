#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBGameplayAbility.h"
#include "CBLifeOnHitAbility.generated.h"

class UGameplayEffect;

/**
 * 적중 회복 패시브 어빌리티 (Life on Hit)
 * - 부여 즉시 활성화(OnGiven)되어 서버에서만 실행(ServerOnly). 폰이 살아 있는 동안 계속 켜져 있음.
 * - 공격 적중 이벤트(Event.Combat.Attack.Hit)마다 LifeOnHit × 맞은 적 수만큼 체력을 회복함. LifeOnHit 이 0 이하면 아무것도 하지 않음.
 * - 회복량의 출처는 LifeOnHit 어트리뷰트뿐. 버스트·아이템·버프 등이 GE 모디파이어로 더하면 그대로 반영됨.
 * - 회복은 GE(SetByCaller Data.Heal)로 적용해 어트리뷰트셋의 체력 상한 클램프를 거침.
 *
 * 회복 GE는 BP 자식(GA_LifeOnHit)에서 지정.
 */
UCLASS()
class CHAINBURST_API UCBLifeOnHitAbility : public UCBGameplayAbility
{
	GENERATED_BODY()

public:
	UCBLifeOnHitAbility();

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~ End UGameplayAbility Interface

	/** 회복 GE (Instant, CurrentHealth Add, 크기는 SetByCaller Data.Heal). BP 자식에서 지정 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|LifeOnHit")
	TSubclassOf<UGameplayEffect> HealEffectClass;

private:
	/** [서버] 적중 이벤트 수신 → LifeOnHit × 맞은 적 수만큼 회복 */
	UFUNCTION()
	void OnAttackHit(FGameplayEventData Payload);
};
