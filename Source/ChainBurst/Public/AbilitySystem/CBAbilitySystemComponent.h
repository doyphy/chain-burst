#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "CBAbilitySystemComponent.generated.h"

/**
 * 프로젝트 공용 어빌리티 시스템 컴포넌트.
 * 입력 태그로 어빌리티를 활성화·해제하고, 로드아웃이 부여한 어빌리티·이펙트를 추적해 한 번에 회수할 수 있게 함.
 */
UCLASS()
class CHAINBURST_API UCBAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UCBAbilitySystemComponent();

	/** 어빌리티	입력이 눌렀을 때 호출하는 함수 (어빌리티 활성화 담당) */
	void OnAbilityInputPressed(const FGameplayTag& InInputTag);

	/** 어빌리티 입력이 떼졌을 때 호출하는 함수 (어빌리티 비활성화 담당) */
	void OnAbilityInputReleased(const FGameplayTag& InInputTag);

#pragma region Loadout Grants
	/**
	 * 로드아웃 부여분 추적 — 부여한 어빌리티·이펙트의 핸들을 모아 두었다가 한 번에 회수함.
	 * Chaser 의 ASC는 PlayerState에서 생성되므로, 캐릭터를 바꿔 다시 스폰할 때 로드아웃 부여분을 회수해야 함.
	 * (로드아웃 밖에서 붙은 것(전투 중 버프 등)은 건드리지 않음)
	 */
public:
	/**
	 * [서버] 로드아웃이 부여하는 어빌리티를 기록하며 부여하는 함수. (서버에서만 실행)
	 * @param InAbilitySpec 부여할 어빌리티 스펙
	 * @return 부여된 어빌리티 스펙 핸들. 권위가 없거나 실패하면 무효 핸들
	 */
	FGameplayAbilitySpecHandle Auth_GiveLoadoutAbility(const FGameplayAbilitySpec& InAbilitySpec);

	/**
	 * [서버] 로드아웃이 적용하는 이펙트를 기록하며 적용하는 함수. (서버에서만 실행)
	 * @param InEffectSpec 적용할 이펙트 스펙
	 * @return 적용된 이펙트 핸들. 즉시(Instant) 이펙트처럼 지속되지 않으면 무효 핸들
	 */
	FActiveGameplayEffectHandle Auth_ApplyLoadoutEffect(const FGameplayEffectSpec& InEffectSpec);

	/**
	 * [서버] 로드아웃이 부여한 어빌리티·이펙트를 전부 회수하는 함수. (서버에서만 실행)
	 * 캐릭터를 바꿔 다시 스폰하기 직전에 호출할 것. 로드아웃 밖에서 붙은 것(전투 중 버프 등)은 건드리지 않음.
	 */
	void Auth_ClearLoadoutGrants();

protected:
	/** 로드아웃이 부여한 어빌리티 핸들. 회수 대상 추적용 */
	TArray<FGameplayAbilitySpecHandle> LoadoutAbilityHandles;

	/** 로드아웃이 적용한 이펙트 핸들. 회수 대상 추적용 (즉시 이펙트는 핸들이 남지 않아 추적되지 않음) */
	TArray<FActiveGameplayEffectHandle> LoadoutEffectHandles;
#pragma endregion
};
