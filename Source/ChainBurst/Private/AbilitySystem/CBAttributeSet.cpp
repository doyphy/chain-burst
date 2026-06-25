// project
#include "AbilitySystem/CBAttributeSet.h"
#include "Characters/CBBaseCharacter.h"
#include "DataAssets/Movement/CBCharacterMovementData.h"
#include "CBGameplayTags.h"

// engine
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

UCBAttributeSet::UCBAttributeSet()
{
	// 기본값 설정
	InitCurrentHealth(1.f);
	InitMaxHealth(1.f);
	InitAttackPower(1.f);
	InitDefensePower(1.f);
}

void UCBAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMovementSpeedAttribute())
	{
		UpdateMovementSpeed(NewValue);
	}
}

void UCBAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UCBAttributeSet, MovementSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCBAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCBAttributeSet, CurrentHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCBAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCBAttributeSet, DefensePower, COND_None, REPNOTIFY_Always);
}

void UCBAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCBAttributeSet, MovementSpeed, OldMovementSpeed);
}

void UCBAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCBAttributeSet, MaxHealth, OldMaxHealth);
}

void UCBAttributeSet::OnRep_CurrentHealth(const FGameplayAttributeData& OldCurrentHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCBAttributeSet, CurrentHealth, OldCurrentHealth);
}

void UCBAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCBAttributeSet, AttackPower, OldAttackPower);
}

void UCBAttributeSet::OnRep_DefensePower(const FGameplayAttributeData& OldDefensePower)
{	
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCBAttributeSet, DefensePower, OldDefensePower);
}

void UCBAttributeSet::UpdateMovementSpeed(float NewValue)
{
	// ASC 가져오기
	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		// 캐릭터 가져오기
		if (ACBBaseCharacter* OwnerCharacter = Cast<ACBBaseCharacter>(ASC->GetAvatarActor()))
		{
			OwnerCharacter->OnMovementSpeedChanged(NewValue);
		}
	}
}

void UCBAttributeSet::OnCharacterSystemReady()
{
	// ASC 가져오기
	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		// 캐릭터 가져오기
		if (ACBBaseCharacter* OwnerCharacter = Cast<ACBBaseCharacter>(ASC->GetAvatarActor()))
		{
			// 이동 데이터 에셋 가져오기
			if (UCBCharacterMovementData* MovementData = OwnerCharacter->GetMovementDataAsset())
			{
				// 이동 속도 설정
				float InitialSpeed = MovementData->GetSpeedForTag(CBGameplayTags::Movement_Run);
				SetMovementSpeed(InitialSpeed);
			}
		}
	}
}
