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

	// ASC 내부 배열을 순회할 때 도중에 배열이 수정되는 것을 방지하기 위해 잠금 (잠금 해제는 함수 종료 시 자동으로 이루어짐)
	ABILITYLIST_SCOPE_LOCK();
	
	// 현재 활성화 가능한 모든 어빌리티를 순회
	for (FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		// 어빌리티의 동적 태그에 입력 태그가 정확히 포함되어 있는지 확인
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InInputTag))
		{
			UE_LOG(LogTemp, Log, TEXT("ASC: %s 의 %s 어빌리티에 %s 입력이 눌렸음."), *GetName(), *AbilitySpec.Ability->GetName(), *InInputTag.ToString());

			// 입력이 눌렸음을 표시하는 플래그 설정 (활성/비활성 무관 — 이후 활성화되는 어빌리티가 홀드 상태를 알 수 있도록)
			AbilitySpec.InputPressed = true;

			// 이미 어빌리티가 활성화 되어 있다면
			if (AbilitySpec.IsActive())
			{
				// 어빌리티 입력 신호가 눌렸음을 알림.
				AbilitySpecInputPressed(AbilitySpec);
				// 입력 눌림 이벤트를 서버로 복제 (서버측 어빌리티 인스턴스/WaitInputPress 태스크가 수신하도록)
				// UE5.5: FGameplayAbilitySpec::ActivationInfo deprecated → 인스턴스별 CurrentActivationInfo 사용 (어빌리티는 InstancedPerActor)
				if (UGameplayAbility* AbilityInstance = AbilitySpec.GetPrimaryInstance())
				{
					InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, AbilitySpec.Handle,
						AbilityInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
				}
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
				// 입력 해제 이벤트를 서버로 복제 (서버측 어빌리티 인스턴스/WaitInputRelease 태스크가 수신하도록)
				// UE5.5: FGameplayAbilitySpec::ActivationInfo deprecated → 인스턴스별 CurrentActivationInfo 사용 (어빌리티는 InstancedPerActor)
				if (UGameplayAbility* AbilityInstance = AbilitySpec.GetPrimaryInstance())
				{
					InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, AbilitySpec.Handle,
						AbilityInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
				}
			}
		}
	}
}

// [서버] 로드아웃이 어빌리티를 부여할 때 경유하는 함수 (부여 + 회수용 핸들 기록)
FGameplayAbilitySpecHandle UCBAbilitySystemComponent::Auth_GiveLoadoutAbility(const FGameplayAbilitySpec& InAbilitySpec)
{
	// 어빌리티 부여는 서버 권위
	if (!IsOwnerActorAuthoritative()) return FGameplayAbilitySpecHandle();

	// 어빌리티 부여
	const FGameplayAbilitySpecHandle GrantedHandle = GiveAbility(InAbilitySpec);

	// 나중에 회수할 수 있도록 핸들 기록
	if (GrantedHandle.IsValid())
	{
		LoadoutAbilityHandles.Add(GrantedHandle);
	}

	return GrantedHandle;
}

// [서버] 로드아웃이 이펙트를 적용할 때 경유하는 함수 (적용 + 회수용 핸들 기록)
FActiveGameplayEffectHandle UCBAbilitySystemComponent::Auth_ApplyLoadoutEffect(const FGameplayEffectSpec& InEffectSpec)
{
	// 이펙트 적용은 서버 권위
	if (!IsOwnerActorAuthoritative()) return FActiveGameplayEffectHandle();

	// 이펙트 적용
	const FActiveGameplayEffectHandle AppliedHandle = ApplyGameplayEffectSpecToSelf(InEffectSpec);

	// 나중에 회수할 수 있도록 핸들 기록.
	// 즉시(Instant) 이펙트는 지속되지 않아 무효 핸들이 돌아오며, 회수할 대상도 없음
	if (AppliedHandle.IsValid())
	{
		LoadoutEffectHandles.Add(AppliedHandle);
	}

	return AppliedHandle;
}

// [서버] 캐릭터를 바꿔 다시 스폰하기 직전에 호출 (ACBLobbyGameMode::Auth_RespawnWithSelectedCharacter)
void UCBAbilitySystemComponent::Auth_ClearLoadoutGrants()
{
	// 회수도 서버 권위
	if (!IsOwnerActorAuthoritative()) return;

	// 이펙트를 먼저 걷어냄 (어빌리티가 참조하는 스탯이 먼저 사라지지 않도록 먼저 처리)
	for (const FActiveGameplayEffectHandle& EffectHandle : LoadoutEffectHandles)
	{
		if (!EffectHandle.IsValid()) continue;

		RemoveActiveGameplayEffect(EffectHandle);
	}
	LoadoutEffectHandles.Reset();

	// 부여했던 어빌리티 제거 (활성 중이면 종료 후 제거됨)
	for (const FGameplayAbilitySpecHandle& AbilityHandle : LoadoutAbilityHandles)
	{
		if (!AbilityHandle.IsValid()) continue;

		ClearAbility(AbilityHandle);
	}
	LoadoutAbilityHandles.Reset();
}
