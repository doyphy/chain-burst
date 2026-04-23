// project
#include "AbilitySystem/CBAbilitySystemComponent.h"

UCBAbilitySystemComponent::UCBAbilitySystemComponent()
{
	SetIsReplicated(true);

	// 복제 모드 설정
	ReplicationMode = EGameplayEffectReplicationMode::Minimal;
}

void UCBAbilitySystemComponent::OnAbilityInputPressed(const FGameplayTag& InInputTag)
{
	// 입력 태그가 유효하지 않으면 함수 종료
	if (!InInputTag.IsValid())
	{
		return;
	}
	// 현재 활성화 가능한 모든 어빌리티를 순회
	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		// 어빌리티의 동적 태그에 입력 태그가 정확히 포함되어 있지 않으면 건너뜀
		if (!AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InInputTag)) continue;
		// 해당 입력 태그와 일치하는 어빌리티를 활성화 시도
		TryActivateAbility(AbilitySpec.Handle);
	}
}

void UCBAbilitySystemComponent::OnAbilityInputReleased(const FGameplayTag& InInputTag)
{
	// 입력 태그가 유효하지 않으면 함수 종료
	if (!InInputTag.IsValid())
	{
		return;
	}
	// 현재 활성화된 모든 어빌리티를 순회
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		// 어빌리티의 동적 태그에 입력 태그가 정확히 포함되어 있지 않으면 건너뜀
		if (!AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InInputTag)) continue;

		// 해당 입력 태그와 일치하는 어빌리티의 입력이 해제되었음을 알림
		// 해당 어빌리티의 InputReleased 함수 호출
		AbilitySpecInputReleased(AbilitySpec);
	}
}