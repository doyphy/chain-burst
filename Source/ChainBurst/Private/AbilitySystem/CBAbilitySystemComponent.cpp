// project
#include "AbilitySystem/CBAbilitySystemComponent.h"

// engine
#include "Abilities/GameplayAbility.h"

UCBAbilitySystemComponent::UCBAbilitySystemComponent()
{
	SetIsReplicated(true);

	// 복제 모드 설정
	ReplicationMode = EGameplayEffectReplicationMode::Minimal;
}

// 캐릭터의 Input_AbilityInputPressed 함수에서 호출되는 함수. 입력 태그에 해당하는 어빌리티를 활성화하거나, 이미 활성화된 어빌리티에 입력이 눌렸음을 알리는 역할을 함.
void UCBAbilitySystemComponent::OnAbilityInputPressed(const FGameplayTag& InInputTag)
{
	// 입력 태그가 유효하지 않으면 함수 종료
	if (!InInputTag.IsValid())
	{
		return;
	}

	// 현재 눌려있는 입력 태그 모음에 해당 입력 태그 추가 (입력감지)
	HeldInputTags.AddTag(InInputTag);

	// 입력 태그가 눌렸음을 알리는 델리게이트 호출 (입력감지)
	OnAbilityInputTagPressed.Broadcast(InInputTag);
	
	// ASC 내부 배열을 순회할 때 도중에 배열이 수정되는 것을 방지하기 위해 잠금 (잠금 해제는 함수 종료 시 자동으로 이루어짐)
	ABILITYLIST_SCOPE_LOCK();
	
	// 현재 활성화 가능한 모든 어빌리티를 순회
	for (FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		// 어빌리티의 동적 태그에 입력 태그가 정확히 포함되어 있는지 확인
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InInputTag))
		{
			UE_LOG(LogTemp, Log, TEXT("ASC: %s 의 %s 어빌리티에 %s 입력이 눌렸음."), *GetName(), *AbilitySpec.Ability->GetName(), *InInputTag.ToString());
			
			// 이미 어빌리티가 활성화 되어 있다면
			if (AbilitySpec.IsActive())
			{
				// 어빌리티 입력 신호가 눌렸음을 알림.
				AbilitySpecInputPressed(AbilitySpec);
			}
			else
			{
				// 해당 입력 태그와 일치하는 어빌리티를 활성화 시도
				TryActivateAbility(AbilitySpec.Handle);
			}
		}
	}
}

// 캐릭터의 Input_AbilityInputReleased 함수에서 호출되는 함수. 입력 태그에 해당하는 어빌리티의 입력이 해제되었음을 알리는 역할을 함. 
void UCBAbilitySystemComponent::OnAbilityInputReleased(const FGameplayTag& InInputTag)
{
	// 입력 태그가 유효하지 않으면 함수 종료
	if (!InInputTag.IsValid())
	{
		return;
	}

	// 현재 눌려있는 입력 태그 모음에서 해당 입력 태그 제거
	HeldInputTags.RemoveTag(InInputTag);
	
	// ASC 내부 배열을 순회할 때 도중에 배열이 수정되는 것을 방지하기 위해 잠금 (잠금 해제는 함수 종료 시 자동으로 이루어짐)
	ABILITYLIST_SCOPE_LOCK();
	
	// 현재 활성화된 모든 어빌리티를 순회
	for (FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		// 어빌리티의 동적 태그에 입력 태그가 정확히 포함되어 있는지 확인
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InInputTag))
		{
			// 입력히 해제되었음을 표시하는 플래그 설정
			AbilitySpec.InputPressed = false;

			if (AbilitySpec.IsActive())
			{
				// 해당 입력 태그와 일치하는 어빌리티의 입력이 해제되었음을 알림
				AbilitySpecInputReleased(AbilitySpec);
			}
		}
	}
}
