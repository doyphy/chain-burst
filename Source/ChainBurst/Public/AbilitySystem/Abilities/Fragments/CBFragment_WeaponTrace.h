#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "CBFragment_WeaponTrace.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UCBCombatComponent;

/**
 * 무기 트레이스 기능 (무기 공격 조각).
 * 무기로 때리는 어빌리티가 멤버로 소유하고 수명 시점마다 호출해 쓰는 기능 객체. 무기 공격에 필요한 세 가지를 함께 처리함.
 * - 무기 검사 : 유효한 무기가 없으면 발동 불가
 * - 트레이스  : [로컬] 몽타주 노티파이의 트레이스 구간 이벤트(Event.Combat.TraceStart/End)로 컴뱃 컴포넌트의 트레이스를 켜고 끔
 * - 데미지 GE : [서버] 히트 이벤트(Event.Combat.Attack.Hit)를 받아 타겟 하나마다 데미지 GE 적용 + 무기 피격 큐
 * 서버의 AI 폰은 로컬·서버가 모두 참이라 한 벌로 플레이어·AI 를 함께 처리함.
 *
 * 사용법: 호스트 어빌리티 생성자에서 CreateDefaultSubobject 로 만들고,
 *        CanActivateAbility → CanActivate / 몽타주 재생 직후 → Start / EndAbility → Stop 을 호출.
 * Outer 가 곧 호스트 어빌리티 (어빌리티 인스턴스 생성 시 함께 만들어지므로 ASC 당 하나).
 */
UCLASS()
class CHAINBURST_API UCBFragment_WeaponTrace : public UObject
{
	GENERATED_BODY()

public:
	/** 무기 검사. CDO 에서도 불릴 수 있으므로 호스트의 CurrentActorInfo 가 아니라 인자 ActorInfo 를 씀 */
	bool CanActivate(const FGameplayAbilityActorInfo* ActorInfo) const;

	/** 공격 시작. [로컬] 트레이스 구간 이벤트 대기 / [서버] 히트 이벤트 대기 */
	void Start();

	/** 공격 종료. 캔슬 등으로 트레이스 종료 노티파이를 못 받았을 수 있으므로 트레이스 강제 정지 */
	void Stop();

protected:
	/** 타겟에게 적용할 데미지 GE 클래스. 비어 있으면 피격 연출 큐도 생략 */
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** 데미지 계수 (FinalDamage = AttackPower * DamageCoefficient - DefensePower) */
	UPROPERTY(EditDefaultsOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float DamageCoefficient = 1.f;

private:
	/** 이 기능을 소유한 어빌리티 (Outer) */
	UGameplayAbility* GetOwningAbility() const;

	/** ActorInfo 의 아바타에서 컴뱃 컴포넌트를 찾음 (없으면 nullptr) */
	static UCBCombatComponent* GetCombatComponent(const FGameplayAbilityActorInfo* ActorInfo);

	UFUNCTION()
	void OnTraceStart(FGameplayEventData Payload);

	UFUNCTION()
	void OnTraceEnd(FGameplayEventData Payload);

	/** [서버] 히트 이벤트 수신 → 타겟마다 데미지 GE 적용 */
	UFUNCTION()
	void OnAttackHit(FGameplayEventData Payload);
};
