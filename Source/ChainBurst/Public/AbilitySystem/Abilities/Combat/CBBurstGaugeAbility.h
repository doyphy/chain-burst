#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBGameplayAbility.h"
#include "CBBurstGaugeAbility.generated.h"

/**
 * 버스트 게이지 적립 패시브 어빌리티
 * - 부여 즉시 활성화(OnGiven)되어 서버에서만 실행(ServerOnly). 폰이 살아 있는 동안 계속 켜져 있음.
 * - 공격 적중 이벤트(Event.Combat.Attack.Hit)마다 맞은 적 수만큼 BurstGauge를 쌓음. MaxBurstGauge에서 멈춤.
 * - 버스트 중(Status.Combat.Burst)에는 쌓지 않음.
 * - 종료 시 게이지를 비우고 버스트 효과를 제거함. 사망(전체 캔슬)·캐릭터 변경(로드아웃 회수)이 모두 이 종료를 거침.
 *
 * 게이지 소모(발동)는 UCBBurstAbility 담당. 적립량은 BP 자식(GA_BurstGauge)에서 지정.
 */
UCLASS()
class CHAINBURST_API UCBBurstGaugeAbility : public UCBGameplayAbility
{
	GENERATED_BODY()

public:
	UCBBurstGaugeAbility();

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

	/** 적 1명을 적중할 때마다 쌓이는 게이지 양 (MaxBurstGauge = 100 기준) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Burst", meta = (ClampMin = "0.0"))
	float GaugeGainPerHit = 5.f;

private:
	/** [서버] 적중 이벤트 수신 → 맞은 적 수만큼 게이지 적립 */
	UFUNCTION()
	void OnAttackHit(FGameplayEventData Payload);
};
