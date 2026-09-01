#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBActionAbility.h"
#include "CBAIAttackAbility.generated.h"

/**
 * [AI 전용] 공격 어빌리티 베이스.
 * BT 태스크가 어빌리티 태그로 직접 활성화(입력 없음).
 *
 * 플레이어 공격(UCBChaserAttackAbility)과 다른 점:
 *  - AI 컨트롤러가 서버 전용이라 NetExecutionPolicy = ServerOnly (몽타주는 GameplayCue 로 전 클라 동기화)
 *  - 콤보 대신 변형 몽타주 무작위 선택(bRandomizeMontage)
 */
UCLASS()
class CHAINBURST_API UCBAIAttackAbility : public UCBActionAbility
{
	GENERATED_BODY()

public:
	UCBAIAttackAbility();

protected:
	//~ Begin UGameplayAbility Interface
		/** 발동 전제 조건 검사 (무기가 없으면 활성화 자체를 막음) */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

	//~ Begin UCBActionAbility Interface
		/** 변형 몽타주를 무작위로 고른다 (bRandomizeMontage 가 꺼져 있으면 0) */
	virtual int32 SelectActionMontageIndex() override;
	/** 모션 워핑 타겟(추격 대상)을 큐 파라미터에 실어 몽타주 재생 중 타겟을 따라가게 한다 */
	virtual void BuildActionCueParameters(FGameplayCueParameters& CueParams) override;
	//~ End UCBActionAbility Interface

	/**
	 * 이 액션 태그에 등록된 변형 몽타주 중 하나를 무작위로 재생할지 여부.
	 * 서버에서만 뽑히고 그 인덱스가 GameplayCue 로 전파되므로 전 클라이언트가 같은 모션을 본다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	bool bRandomizeMontage = false;

#pragma region MotionWarp
	/** 몽타주 재생 중 타겟 쪽으로 접근·정렬시키는 모션 워핑 설정 */
protected:
	/**
	 * 공격 몽타주 재생 중 타겟을 향해 워프할지 여부.
	 * 켜면 큐 파라미터에 타겟 컴포넌트를 실어 보내고, 전 클라이언트가 그 대상을 매 프레임 추종.
	 * 실제 워프 구간·강도는 몽타주의 Motion Warping 노티파이가 결정. (구간을 선딜에만 두면 살짝만 따라감).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|MotionWarp")
	bool bWarpToTarget = true;

	/** 타겟에서 이만큼 떨어진 지점까지만 접근 (cm). 공격 사거리와 맞추기. (관통 방지) */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|MotionWarp", meta = (EditCondition = "bWarpToTarget", ClampMin = "0.0"))
	float WarpStopDistance = 100.f;
#pragma endregion

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
