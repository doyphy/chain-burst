#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBGameplayAbility.h"
#include "CBGAJump.generated.h"

/**
 * 점프 어빌리티 (C++ 최종 엣지).
 * CMC의 Jump()/StopJumping()만 트리거한다 — 몽타주 없음. 공중 애니메이션(점프 상승/낙하)은
 * ABP 상태 머신이 IsInAir()/GetVerticalVelocity() 데이터로 처리한다 (낙하 등 비점프 공중 상태와 공용).
 * Jump()가 세우는 bPressedJump는 CMC 압축 플래그로 ServerMove에 실려 복제되므로 별도 동기화 코드가 불필요.
 * 대시 중에는 루트모션이 수직 속도를 덮어써 점프가 씹히므로 ActivationBlockedTags(Status.Movement.Dashing)로 차단.
 */
UCLASS()
class CHAINBURST_API UCBGAJump : public UCBGameplayAbility
{
	GENERATED_BODY()

public:
	UCBGAJump();

protected:
	//~ Begin UGameplayAbility Interface
	/** CanJump 검사 — 공중 재점프 등 불가 상황이면 활성화 자체를 차단 (불필요한 활성화 RPC 방지) */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End UGameplayAbility Interface
};
