#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBInputActionAbility.h"
#include "CBGADash.generated.h"

/**
 * 대시 어빌리티 (입력 액션 엣지).
 * 재생 인덱스는 전투 상태(Status.Combat.InCombat)로 분기 — 비전투면 인덱스 0(일반 대시), 전투면 인덱스 1(전투 대시).
 *
 * 몽타주는 전방 1방향뿐이고, 실제 방향·거리는 모션 워핑으로 정함.
 * 방향 = 누른 순간의 카메라 기준 이동 입력(입력 없으면 액터 정면), 거리 = DashDistance.
 * 카메라 기준 입력은 소유 클라만 알 수 있으므로, 원격 클라는 방향을 TargetData(Server RPC)로 보내고
 * 서버는 받은 뒤에 재생함. (호스트는 서버 자신이라 전송 없이 바로 재생)
 * 몽타주 종료 후 전방 Sprint 루프로 자연 연결.
 *
 * Sprint는 대시에 종속된다: 대시가 성공하면 이 어빌리티가 Sprint 어빌리티(GA_Sprint)를 태그로 직접 활성화함,
 *
 * GA_Sprint 쪽은 ActivationRequiredTags(Status.Movement.Dashing)로 대시 없이는 활성화되지 않게 막는다.
 * 쿨다운은 GAS 표준 CooldownGameplayEffectClass 사용 — 쿨다운 중엔 대시가 실패하므로 Sprint도 함께 발동 불가.
 *
 * 활성 동안 Status.Movement.Dashing 태그를 부여해(ActivationOwnedTags) 대시 전용 감속 판별과 Sprint 활성화 조건에 사용함.
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
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

	//~ Begin UCBActionAbility Interface
	virtual int32 SelectActionMontageIndex() override;
	virtual void BuildActionCueParameters(FGameplayCueParameters& CueParams) override;
	//~ End UCBActionAbility Interface

	/** 대시 성공 시 함께 활성화할 Sprint 어빌리티의 식별 태그 (AssetTags 기준). */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Dash", meta = (Categories = "Ability"))
	FGameplayTag SprintAbilityTag;

	/** 모션 워핑으로 이동할 대시 거리 (유닛). 방향은 누른 순간의 카메라 기준 이동 입력. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Dash")
	float DashDistance = 700.0f;

private:
	/** [로컬 전용] 카메라 기준 이동 입력으로 대시 방향을 정하는 함수 (입력이 없으면 액터 정면) */
	FVector Local_CalculateDashDirection() const;

	/** [로컬 전용] 정한 대시 방향을 서버로 보내는 함수 (TargetData — 활성화 예측 키와 함께 전달됨) */
	void Local_SendDashDirectionToServer(const FVector& InDirection);

	/** [서버 전용] 원격 클라가 보낸 대시 방향 수신 콜백 */
	void Auth_HandleDashDirectionReceived(const FGameplayAbilityTargetDataHandle& InDataHandle, FGameplayTag InApplicationTag);

	/** 방향이 정해진 뒤 대시 시작 (몽타주 재생 + Sprint 활성화) */
	void StartDash(const FVector& InDirection);

	/** 이번 대시 방향 (재생 직전에 확정, BuildActionCueParameters가 워프 타겟 계산에 사용) */
	FVector DashDirection = FVector::ZeroVector;
};
