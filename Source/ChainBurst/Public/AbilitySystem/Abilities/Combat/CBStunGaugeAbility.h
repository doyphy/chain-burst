#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBGameplayAbility.h"
#include "CBStunGaugeAbility.generated.h"

/**
 * 기절 게이지 적립 패시브 어빌리티 (보스용)
 * - 부여 즉시 활성화(OnGiven)되어 서버에서만 실행(ServerOnly). 폰이 살아 있는 동안 계속 켜져 있음.
 * - 적립 1 - 피격: 피격 반응 이벤트(Event.Combat.HitReact)마다 GaugeGainPerHit.
 *   슈퍼아머로 반응이 막혀도 이벤트는 오므로 보스가 스킬을 쓰는 중에 맞아도 쌓임.
 * - 적립 2 - 자기 어빌리티: 정상 종료(캔슬 제외)된 어빌리티의 식별 태그를 AbilityGaugeGains 에서 찾아 그 양만큼.
 *   표에 없는 어빌리티(피격·기절 등)는 무시. 스킬을 끝까지 쓴 뒤에 쌓이므로 게이지를 채운 스킬이 기절에 끊기지 않음.
 * - 기절 중(Status.Combat.Stunned)에는 쌓지 않음.
 * - MaxStunGauge 에 닿으면 게이지를 비우고 기절 이벤트(Event.Combat.Stunned)를 발행 → 기절 어빌리티가 발동.
 *
 * 기절 동작 자체(공격 캔슬·몽타주·상태 태그)는 이벤트 액션 어빌리티 BP(GA_Stun)가 담당. 적립량은 BP 자식(GA_StunGauge)에서 지정.
 * AI 는 ASC 가 폰과 함께 사라지므로 종료 시 게이지를 초기화하지 않음.
 */
UCLASS()
class CHAINBURST_API UCBStunGaugeAbility : public UCBGameplayAbility
{
	GENERATED_BODY()

public:
	UCBStunGaugeAbility();

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

	/** 한 번 맞을 때마다 쌓이는 게이지 양 (MaxStunGauge = 100 기준) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Stun", meta = (ClampMin = "0.0"))
	float GaugeGainPerHit = 5.f;

	/**
	 * 정상 종료된 자기 어빌리티의 식별 태그별 적립량 (MaxStunGauge = 100 기준).
	 * 어빌리티 Asset Tags 와 정확히 같은 태그로 찾음 (예: Ability.Combat.Attack.Skill.A → 20). 표에 없으면 적립 없음.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Stun", meta = (Categories = "Ability"))
	TMap<FGameplayTag, float> AbilityGaugeGains;

private:
	/** [서버] 피격 반응 이벤트 수신 → GaugeGainPerHit 적립 */
	UFUNCTION()
	void OnHitReact(FGameplayEventData Payload);

	/** [서버] 어빌리티 종료 알림 → 정상 종료된 어빌리티면 표에 있는 양만큼 적립 */
	void OnAbilityEnded(const FAbilityEndedData& InEndedData);

	/**
	 * [서버] 게이지 적립. 가득 차면 게이지를 비우고 기절 이벤트를 발행.
	 * @param InGain 적립량 (0 이하면 무시)
	 */
	void AddStunGauge(float InGain);

	/** 어빌리티 종료 알림 구독 핸들 (EndAbility 에서 해제) */
	FDelegateHandle AbilityEndedHandle;
};
