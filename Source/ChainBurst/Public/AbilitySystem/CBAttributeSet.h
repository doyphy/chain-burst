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

	void UpdateMovementSpeed(float NewValue);

	/**
	 * [서버] 사망 이벤트(Event.Combat.Death)를 발행하는 함수.
	 * 사망 어빌리티가 이 태그를 트리거로 발동된다. 이미 Status.Dead면 재발행하지 않는다.
	 * @param InInstigator 사망을 유발한 액터
	 */
	void Auth_SendDeathEvent(AActor* InInstigator);
};
