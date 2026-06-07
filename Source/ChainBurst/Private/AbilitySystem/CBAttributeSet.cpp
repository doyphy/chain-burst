// project
#include "AbilitySystem/CBAttributeSet.h"

// engine
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

UCBAttributeSet::UCBAttributeSet()
{
	// 기본값 설정
	InitCurrentHealth(1.f);
	InitMaxHealth(1.f);
}

void UCBAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
}

void UCBAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UCBAttributeSet, MovementSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCBAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCBAttributeSet, CurrentHealth, COND_None, REPNOTIFY_Always);
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
