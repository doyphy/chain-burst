// project
#include "Characters/CBRogueCharacter.h"
#include "DataAssets/Loadout/CBRogueLoadout.h"
#include "Components/Combat/CBRogueCombatComponent.h"

ACBRogueCharacter::ACBRogueCharacter()
{
	// Combat Component 생성 (일반 잡몹 전용). ASC·AttributeSet·이동/빙의 설정은 AI 베이스에서 처리.
	RogueCombatComponent = CreateDefaultSubobject<UCBRogueCombatComponent>(TEXT("CBRogueCombatComponent"));
}

TSoftObjectPtr<UCBCharacterLoadout> ACBRogueCharacter::GetAILoadout() const
{
	return RogueLoadout;
}

UCBCombatComponent* ACBRogueCharacter::GetCBCombatComponent() const
{
	return RogueCombatComponent.Get();
}
