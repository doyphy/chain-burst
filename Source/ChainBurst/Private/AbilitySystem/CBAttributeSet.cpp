// project
#include "AbilitySystem/CBAttributeSet.h"

// engine
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

UCBAttributeSet::UCBAttributeSet()
{
	
}

void UCBAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
}

void UCBAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UCBAttributeSet, MovementSpeed, COND_None, REPNOTIFY_Always);
}

void UCBAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCBAttributeSet, MovementSpeed, OldMovementSpeed);
}
