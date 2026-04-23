// project
#include "DataAssets/Loadout/CBChaserLoadout.h"
#include "AbilitySystem/Abilities/CBChaserGameplayAbility.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"

bool FCBChaserAbilitySet::IsValid() const
{
	return InputTag.IsValid() && AbilityToGrant;
}

void UCBChaserLoadout::Auth_GrantAbilitiesToASC(UCBAbilitySystemComponent* InASCToGive, int32 ApplyLevel)
{
	// 액티브, 패시브, 반응형 어빌리티 부여 (부모 함수 호출)
	Super::Auth_GrantAbilitiesToASC(InASCToGive, ApplyLevel);
	
	// ChaserAbilitySets 배열을 순회하며 각 어빌리티 세트를 처리
	for(const FCBChaserAbilitySet& AbilitySet : ChaserAbilitySets)
	{
		// 입력 태그와 어빌리티 클래스가 모두 유효한지 검사, 유효하지 않으면 건너뜀
		if (!AbilitySet.IsValid()) continue;
		
		// 어빌리티 스펙 생성 (부여할 어빌리티 클래스 기반)
		FGameplayAbilitySpec AbilitySpec(AbilitySet.AbilityToGrant);
		// 어빌리티의 소스 오브젝트를 현재 어빌리티 시스템 컴포넌트의 아바타로 지정
		AbilitySpec.SourceObject = InASCToGive->GetAvatarActor();
		// 어빌리티 레벨 설정
		AbilitySpec.Level = ApplyLevel;
		// 입력 태그를 동적 어빌리티 태그에 추가 (입력 매핑 등에서 활용)
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(AbilitySet.InputTag);

		// 어빌리티 시스템 컴포넌트에 어빌리티 부여
		InASCToGive->GiveAbility(AbilitySpec);
	}
}
