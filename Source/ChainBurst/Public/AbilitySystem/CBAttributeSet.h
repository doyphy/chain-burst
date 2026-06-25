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
	/** 속성 값이 변경된 후 실행되는 함수 */
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	
	ATTRIBUTE_ACCESSORS(UCBAttributeSet, MovementSpeed)

	ATTRIBUTE_ACCESSORS(UCBAttributeSet, MaxHealth)

	ATTRIBUTE_ACCESSORS(UCBAttributeSet, CurrentHealth)

	ATTRIBUTE_ACCESSORS(UCBAttributeSet, AttackPower)

	ATTRIBUTE_ACCESSORS(UCBAttributeSet, DefensePower)
	
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
	
	void UpdateMovementSpeed(float NewValue);

};
