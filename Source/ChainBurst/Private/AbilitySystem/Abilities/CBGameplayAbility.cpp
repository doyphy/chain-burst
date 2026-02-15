// project
#include "AbilitySystem/Abilities/CBGameplayAbility.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "Components/Combat/CBCombatComponent.h"

void UCBGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	
	// 어빌리티 활성화 정책이 OnGiven인 경우
	if (AbilityActivationPolicy == ECBAbilityActivationPolicy::OnGiven)
	{
		// ActorInfo가 유효하고, 해당 어빌리티(Spec)가 아직 활성화되지 않은 경우
		if(ActorInfo && !Spec.IsActive())
		{
			// AbilitySystemComponent를 통해 해당 어빌리티(Spec)를 즉시 활성화 시도
			ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
		}
	}
}

void UCBGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	
	// 어빌리티 활성화 정책이 OnGiven인 경우
	if(AbilityActivationPolicy == ECBAbilityActivationPolicy::OnGiven)
	{
		// ActorInfo가 유효한 경우
		if(ActorInfo)
		{
			// AbilitySystemComponent에서 해당 어빌리티(Handle)를 제거
			ActorInfo->AbilitySystemComponent->ClearAbility(Handle);
		}
	}
}

UCBCombatComponent* UCBGameplayAbility::GetCBCombatComponentFromActorInfo() const
{
	return GetAvatarActorFromActorInfo()->FindComponentByClass<UCBCombatComponent>();
}

UCBAbilitySystemComponent* UCBGameplayAbility::GetCBAbilitySystemComponentFromActorInfo() const
{
	return Cast<UCBAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent);
}
