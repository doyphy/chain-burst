#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "CBAttributeSet.generated.h"

// Attribute 매크로, Getter, Setter, Initter 정의
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class CHAINBURST_API UCBAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
	UCBAttributeSet();
	
public:
	/** [서버/클라] 어트리뷰트 값이 최종 확정된 후 호출.*/
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	/** [서버] GE가 어트리뷰트에 적용된 직후 호출. */
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;
	
	ATTRIBUTE_ACCESSORS(UCBAttributeSet, MovementSpeed)

	ATTRIBUTE_ACCESSORS(UCBAttributeSet, MaxHealth)

	ATTRIBUTE_ACCESSORS(UCBAttributeSet, CurrentHealth)

	ATTRIBUTE_ACCESSORS(UCBAttributeSet, AttackPower)

	ATTRIBUTE_ACCESSORS(UCBAttributeSet, DefensePower)

	ATTRIBUTE_ACCESSORS(UCBAttributeSet, AttackSpeed)

	ATTRIBUTE_ACCESSORS(UCBAttributeSet, BurstGauge)

	ATTRIBUTE_ACCESSORS(UCBAttributeSet, LifeOnHit)

	/** 버스트 게이지 최대값. (차는 속도는 UCBBurstGaugeAbility으로 조절하므로 상수로 고정함) */
	static constexpr float MaxBurstGauge = 100.f;

	/** 캐릭터 시스템 준비 완료 시 캐릭터에서 호출되는 함수 (초기화 작업) */
	void OnCharacterSystemReady();
	
public:
	UPROPERTY(BlueprintReadOnly, Category = "Movement", ReplicatedUsing = OnRep_MovementSpeed)
	FGameplayAttributeData MovementSpeed;

	UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;

	UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing = OnRep_CurrentHealth)
	FGameplayAttributeData CurrentHealth;

	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_AttackPower)
	FGameplayAttributeData AttackPower;

	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_DefensePower)
	FGameplayAttributeData DefensePower;

	/** 공격 속도 배율 (1.0 = 기본) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_AttackSpeed)
	FGameplayAttributeData AttackSpeed;

	/**
	 * 버스트 게이지 (0 ~ MaxBurstGauge). 가득 차면 버스트 발동 가능.
	 * 서버만 기록함 (적중 이벤트는 서버에서 검증 후 적용)
	 * 적립·초기화는 UCBBurstGaugeAbility, 소모는 UCBBurstAbility.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_BurstGauge)
	FGameplayAttributeData BurstGauge;

	/**
	 * 적 1명을 적중할 때마다 회복하는 체력 (0 이하 = 효과 없음).
	 * 출처(버스트·아이템·버프 등)가 GE 모디파이어로 더함. 소비는 UCBLifeOnHitAbility(서버).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_LifeOnHit)
	FGameplayAttributeData LifeOnHit;

	/** 리플리케이션 설정 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION()
	virtual void OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
	
	UFUNCTION()
	virtual void OnRep_CurrentHealth(const FGameplayAttributeData& OldCurrentHealth);

	UFUNCTION()
	virtual void OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower);

	UFUNCTION()
	virtual void OnRep_DefensePower(const FGameplayAttributeData& OldDefensePower);

	UFUNCTION()
	virtual void OnRep_AttackSpeed(const FGameplayAttributeData& OldAttackSpeed);

	UFUNCTION()
	virtual void OnRep_BurstGauge(const FGameplayAttributeData& OldBurstGauge);

	UFUNCTION()
	virtual void OnRep_LifeOnHit(const FGameplayAttributeData& OldLifeOnHit);

	void UpdateMovementSpeed(float NewValue);

	/**
	 * [서버] 사망 이벤트(Event.Combat.Death)를 발행하는 함수.
	 * 사망 어빌리티가 이 태그를 트리거로 발동된다. 이미 Status.Dead면 재발행하지 않는다.
	 * @param InInstigator 사망을 유발한 액터
	 */
	void Auth_SendDeathEvent(AActor* InInstigator);
};
