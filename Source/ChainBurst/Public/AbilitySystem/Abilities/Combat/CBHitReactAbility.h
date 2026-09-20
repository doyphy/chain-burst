#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBEventActionAbility.h"
#include "CBHitReactAbility.generated.h"

/**
 * 피격 반응 어빌리티
 * - Event.Combat.HitReact 게임플레이 이벤트로 자동 발동 (입력 불필요)
 * - 발동 시 진행 중인 공격 어빌리티(Ability.Combat.Attack)를 캔슬하고 피격 몽타주(Action.Combat.HitReact) 재생
 * - 활성 구간 동안 경직 상태(Status.Combat.Staggered)를 소유 → AI 컨트롤러가 블랙보드로 미러링해 BT가 경직 분기로 빠짐
 * - 경직 중 재피격 시 재발동(bRetriggerInstancedAbility) → 연속으로 맞으면 그만큼 경직이 이어짐
 * - 단, 연쇄 경직이 MaxChainStaggerTime을 넘으면 ChainStaggerCooldown 동안 발동을 차단 (무한 경직 방지)
 * - 시전자가 Status.Combat.SuperArmor를 들고 있으면 발동 자체가 차단됨 (ActivationBlockedTags) → 스킬이 피격에 끊기지 않음
 * - 타격 반대 방향으로 밀려나는 넉백을 GAS 루트모션 소스로 적용 (기본은 AI 전용)
 */
UCLASS()
class CHAINBURST_API UCBHitReactAbility : public UCBEventActionAbility
{
	GENERATED_BODY()

public:
	UCBHitReactAbility();
	
protected:
	//~ Begin UGameplayAbility Interface
		/** 발동 가능 여부. 연쇄 경직이 한도를 넘은 차단 구간이면 거부한다 (상태를 바꾸지 않는 순수 읽기). */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
		/** 몽타주 재생 전에 연쇄 경직 상태를 먼저 갱신한다. */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
		/** 경직이 끝난 시각을 기록한다 (다음 피격이 같은 연쇄인지 판정하는 기준). */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

	//~ Begin UCBActionAbility Interface
	virtual int32 SelectActionMontageIndex() override;
	//~ End UCBActionAbility Interface

#pragma region ChainStagger
	/**
	 * 연쇄 경직 제한. 맞는 동안 계속 경직되면 영영 빠져나오지 못하므로, 일정 시간 이어지면 잠시 반응을 끊어 탈출구를 만듦.
	 * 차단 구간에도 데미지 GE는 그대로 적용되고 반응 모션만 생략 (슈퍼아머와 같은 동작).
	 *
	 * 상태를 어빌리티 멤버로 두는 이유: 소비자가 이 클래스 하나뿐이라 태그로 만들 이유가 없음.
	 * InstancedPerActor 라 인스턴스가 ASC 당 하나이므로, 발동 사이에는 값이 유지되고 개체끼리는 섞이지 않음.
	 */
protected:
	/** 연쇄 경직이 이 시간(초)을 넘으면 반응을 차단함. 0 이하면 제한 없음. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Stagger", meta = (ClampMin = "0.0"))
	float MaxChainStaggerTime = 5.f;

	/** 한도를 넘었을 때 반응을 차단하는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Stagger", meta = (ClampMin = "0.0"))
	float ChainStaggerCooldown = 5.f;

	/** 직전 경직이 끝나고 이 시간(초) 안에 다시 맞으면 같은 연쇄로 봄. 지나서 맞으면 연쇄를 새로 시작. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Stagger", meta = (ClampMin = "0.0"))
	float ChainBreakTime = 1.0f;

private:
	/** 차단 구간인지 판정 (연쇄 경과가 [MaxChainStaggerTime, MaxChainStaggerTime + ChainStaggerCooldown) 이면 참). */
	bool IsChainStaggerBlocked(float InNow) const;

	/** 이번 피격이 같은 연쇄인지 판정해, 아니면 연쇄를 새로 시작함. */
	void UpdateChainStagger(float InNow);

	/** 현재 연쇄가 시작된 시각(초). 음수면 진행 중인 연쇄 없음. */
	float ChainStartTime = -1.f;

	/** 마지막 경직이 끝난 시각(초). 음수면 기록 없음. */
	float LastStaggerEndTime = -1.f;
#pragma endregion

#pragma region Knockback
	/**
	 * 피격 넉백. 타격 반대 방향으로 밀려나는 연출을 GAS 루트모션 소스로 적용.
	 * 충돌은 CMC 가 매 프레임 처리함 (막히면 미끄러지고, 캐릭터끼리는 밀지 않음).
	 */
protected:
	/** 밀려날 거리(cm). 0 이하면 넉백 없음. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Knockback", meta = (ClampMin = "0.0"))
	float KnockbackDistance = 50.f;

	/** 밀려나는 데 걸리는 시간(초). 너무 짧으면 다른 클라이언트에서 보간에 뭉개져 보이지 않음. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Knockback", meta = (ClampMin = "0.01"))
	float KnockbackDuration = 0.2f;

	/**
	 * 플레이어가 조종하는 폰을 넉백에서 제외할지 여부 (기본값 : true).
	 * 3인칭 액션에서 플레이어가 밀리면 조작감이 나빠지므로 AI 전용이 기본.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Knockback")
	bool bKnockbackAIOnly = true;

	/** 밀려날 지점이 내비메시 밖이면 넉백을 생략할지 여부 (절벽 추락·경로 상실 방지). */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Knockback")
	bool bClampToNavMesh = true;

	/** 내비메시 투영 허용 오차(cm). bClampToNavMesh 일 때만 사용. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Knockback", meta = (EditCondition = "bClampToNavMesh", ClampMin = "0.0"))
	float NavProjectExtent = 100.f;

private:
	/** 넉백 적용. */
	void ApplyKnockback();

	/**
	 * 넉백 방향 계산 (타격 지점 → 공격자 위치 → 자기 뒤쪽 순으로 계산).
	 * @return 수평 단위 벡터. 영벡터면 넉백 없음.
	 */
	FVector ComputeKnockbackDirection() const;
#pragma endregion
};
