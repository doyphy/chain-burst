// project
#include "DataAssets/Loadout/CBCharacterLoadout.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/CBGameplayAbility.h"
#include "Components/Combat/CBCombatComponent.h"

// engine

void UCBCharacterLoadout::Auth_GrantAbilitiesToASC(UCBAbilitySystemComponent* InASCToGive, int32 ApplyLevel)
{
	// 어빌리티 시스템 컴포넌트가 유효한지 확인
	check(InASCToGive);
	
	// 액티브 어빌리티 부여
	GrantAbilities(ActiveAbilities, InASCToGive, ApplyLevel);
	// 패시브 어빌리티 부여
	GrantAbilities(PassiveAbilities, InASCToGive, ApplyLevel);
	// 반응형 어빌리티 부여
	GrantAbilities(ReactiveAbilities, InASCToGive, ApplyLevel);
}

void UCBCharacterLoadout::Auth_RegisterWeaponsToCombatComponent(UCBCombatComponent* InCombatComponent)
{
	check(InCombatComponent);

	// 등록할 무기가 없으면 함수 종료
	if (!WeaponData.IsValid()) return;

	// 컴뱃 컴포넌트에 무기 등록
	InCombatComponent->Auth_RegisterWeapon(WeaponData);
}

void UCBCharacterLoadout::GrantAbilities(const TArray<TSubclassOf<UCBGameplayAbility>>& InAbilitiesToGive,
                                         UCBAbilitySystemComponent* InASCToGive, int32 ApplyLevel)
{
	// 부여할 어빌리티가 없으면 함수 종료
	if (InAbilitiesToGive.IsEmpty()) return;
	
	// 배열을 순회하며 각 어빌리티를 부여
	for (const auto& Ability : InAbilitiesToGive)
	{
		// 어빌리티 클래스가 유효하지 않으면 건너뜀
		if (!Ability) continue;
		// 어빌리티 스펙 생성
		FGameplayAbilitySpec AbilitySpec(Ability);
		// 어빌리티의 소스 오브젝트를 현재 어빌리티 시스템 컴포넌트의 아바타로 지정
		AbilitySpec.SourceObject = InASCToGive->GetAvatarActor();
		// 어빌리티 레벨 설정
		AbilitySpec.Level = ApplyLevel;

		// 어빌리티 시스템 컴포넌트에 어빌리티 부여
		// 어빌리티의 활성화 정책이 OnGiven 이면 부여 즉시 TryActivateAbility 호출함 (CBGameplayAbility 참고)
		InASCToGive->GiveAbility(AbilitySpec);
	}
}