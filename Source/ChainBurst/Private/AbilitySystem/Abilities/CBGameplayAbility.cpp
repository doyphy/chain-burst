// project
#include "AbilitySystem/Abilities/CBGameplayAbility.h"
#include "CBGameplayTags.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "Components/Combat/CBCombatComponent.h"
#include "Characters/CBBaseCharacter.h"

UCBGameplayAbility::UCBGameplayAbility()
{
	// [기본 값] 액터당 하나씩 인스턴스를 생성하도록 설정
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

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

bool UCBGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	
	if (ASC)
	{
		// 사망 상태면 사망 관련 어빌리티를 제외한 전부 차단.
		// 모든 어빌리티의 루트라 여기 한 곳만 둠.
		if (!bActivatableWhileDead && ASC->HasMatchingGameplayTag(CBGameplayTags::Status_Dead))
		{
			return false;
		}

		// 현재 어빌리티 태그 (Asset Tags)의 부모 태그들 가져오기
		FGameplayTagContainer ParentTags = GetAssetTags().GetGameplayTagParents();

		// ASC의 Block된 태그에 내 부모 태그가 있는지 검사 
		if (ASC->AreAbilityTagsBlocked(ParentTags))
		{
			// 어빌리티 활성화 거부
			return false;
		}
	}
	// 어빌리티 활성화 허용
	return true;
}

UCBCombatComponent* UCBGameplayAbility::GetCBCombatComponentFromActorInfo() const
{
	if (ACBBaseCharacter* BaseChar = Cast<ACBBaseCharacter>(GetAvatarActorFromActorInfo()))
	{
		return BaseChar->GetCBCombatComponent();
	}
	return nullptr;
}

UCBAbilitySystemComponent* UCBGameplayAbility::GetCBAbilitySystemComponentFromActorInfo() const
{
	return Cast<UCBAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent);
}

UCBActionComponent* UCBGameplayAbility::GetCBActionComponentFromActorInfo() const
{
	if (ACBBaseCharacter* BaseChar = Cast<ACBBaseCharacter>(GetAvatarActorFromActorInfo()))
	{
		return BaseChar->GetCBActionComponent();
	}
	return nullptr;
}
