// project
#include "DataAssets/Loadout/CBChaserLoadout.h"
#include "AbilitySystem/Abilities/CBChaserGameplayAbility.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "Characters/CBChaserCharacter.h"
#include "Components/UI/CBUIComponent.h"

bool FCBInputAbilitySet::IsValid() const
{
	return InputTag.IsValid() && AbilityToGrant;
}

void UCBChaserLoadout::Auth_GrantAbilitiesToASC(UCBAbilitySystemComponent* InASCToGive, int32 ApplyLevel)
{
	// 액티브, 패시브, 반응형 어빌리티 부여 (부모 함수 호출)
	Super::Auth_GrantAbilitiesToASC(InASCToGive, ApplyLevel);
	
	// InputAbilitySets 배열을 순회하며 각 어빌리티 세트를 처리
	for(const FCBInputAbilitySet& AbilitySet : InputAbilitySets)
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

void UCBChaserLoadout::Local_ApplyToCharacter(ACBChaserCharacter* InCharacter)
{
	// 캐릭터 유효성 검사
	if (!InCharacter) return;

	// 입력 설정 주입 (세터 내부에서 입력 컴포넌트 준비 여부를 확인해 지연 바인딩 수행)
	InCharacter->SetInputConfig(InputConfig);

	// HUD 체력 위젯 클래스 주입 (위젯 생성은 준비 완료 후 UI 컴포넌트가 수행)
	if (UCBUIComponent* UIComponent = InCharacter->GetCBUIComponent())
	{
		UIComponent->SetHUDWidgetClass(HUDHealthWidgetClass);
	}
}
