// project
#include "AbilitySystem/Abilities/Combat/CBChaserAttackAbility.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"

void UCBChaserAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UCBChaserAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	
	if (UCBAbilitySystemComponent* CBASC = GetCBAbilitySystemComponentFromActorInfo())
	{
		// 아직도 입력중이라면 (예: 플레이어가 공격 버튼을 계속 누르고 있다면)
		if (BoundInputTag.IsValid() && CBASC->HeldInputTags.HasTagExact(BoundInputTag))
		{
			// 무한 루프 방지 (0.1초 후 실행해서 호출이 과하게 쌓이는 것 방지)
			if (UWorld* World = GetWorld())
			{
				FTimerHandle RetryHandle;
				World->GetTimerManager().SetTimer(
					RetryHandle,
					FTimerDelegate::CreateWeakLambda(CBASC, [CBASC, Handle]()
					{
						CBASC->TryActivateAbility(Handle); // 어빌리티 다시 활성화 시도
					}),
					0.1f, // 0.1초 후 실행.
					false // 반복하지 않음.
				);
			}
		}
	}
}
