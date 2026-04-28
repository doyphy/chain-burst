// project
#include "PlayerState/CBPlayerState.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem//CBAttributeSet.h"

ACBPlayerState::ACBPlayerState()
{
	// ASC 생성
	CBAbilitySystemComponent = CreateDefaultSubobject<UCBAbilitySystemComponent>(TEXT("CBAbilitySystemComponent"));
	// 복제 설정
	CBAbilitySystemComponent->SetIsReplicated(true);
	// 플레이어 캐릭터의 ASC 복제 모드는 Mixed로 설정
	CBAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// AttributeSet 생성
	CBAttributeSet = CreateDefaultSubobject<UCBAttributeSet>(TEXT("CBAttributeSet"));
}

UAbilitySystemComponent* ACBPlayerState::GetAbilitySystemComponent() const
{
	return Cast<UAbilitySystemComponent>(CBAbilitySystemComponent);
}
