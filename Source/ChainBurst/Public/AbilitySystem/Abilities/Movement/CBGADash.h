#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBInputActionAbility.h"
#include "CBGADash.generated.h"

/**
 * 전방 대시 어빌리티 (입력 액션 엣지).
 * Input.Action.Sprint에 바인딩되어, 키 입력 시 전방 대시 몽타주(Action.Movement.Dash)를 재생한다.
 * 재생 인덱스는 전투 상태(Status.Combat.InCombat)로 분기 — 비전투면 인덱스 0(일반 대시), 전투면 인덱스 1(전투 대시).
 * Sprint 루프가 전방 질주 동작뿐이라 대시도 전방 1방향만 사용 — 몽타주 종료 후 전방 Sprint 루프로 자연 연결.
 * Sprint는 대시에 종속된다: 대시가 성공하면 이 어빌리티가 Sprint 어빌리티(GA_Sprint)를 태그로 직접 활성화하고,
 * GA_Sprint 쪽은 ActivationRequiredTags(Status.Movement.Dashing)로 대시 없이는 활성화되지 않게 막는다.
 * 쿨다운은 GAS 표준 CooldownGameplayEffectClass 사용 — 쿨다운 중엔 대시가 실패하므로 Sprint도 함께 발동 불가.
 * 활성 동안 Status.Movement.Dashing 태그를 부여해(ActivationOwnedTags) 대시 전용 감속 판별과 Sprint 활성화 조건에 사용한다.
 */
UCLASS()
class CHAINBURST_API UCBGADash : public UCBInputActionAbility
{
	GENERATED_BODY()

public:
	UCBGADash();

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~ End UGameplayAbility Interface

	//~ Begin UCBActionAbility Interface
	virtual int32 SelectActionMontageIndex() const override;
	//~ End UCBActionAbility Interface

	/**
	 * 대시 성공 시 함께 활성화할 Sprint 어빌리티의 식별 태그 (AssetTags 기준).
	 * 입력 순회 순서(로드아웃 부여 순서)에 의존하지 않고 "대시 성공 → Sprint 활성화"를 보장하기 위해 대시가 직접 트리거한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Dash", meta = (Categories = "Ability"))
	FGameplayTag SprintAbilityTag;
};
