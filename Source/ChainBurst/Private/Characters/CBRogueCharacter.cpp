// project
#include "Characters/CBRogueCharacter.h"
#include "DataAssets/Loadout/CBRogueLoadout.h"
#include "Components/Combat/CBRogueCombatComponent.h"
#include "Controllers/CBRogueController.h"

ACBRogueCharacter::ACBRogueCharacter()
{
	// Combat Component 생성 (일반 잡몹 전용). ASC·AttributeSet·이동/빙의 설정은 AI 베이스에서 처리.
	RogueCombatComponent = CreateDefaultSubobject<UCBRogueCombatComponent>(TEXT("CBRogueCombatComponent"));

	// 빙의할 AI 컨트롤러 지정 (자동 빙의 설정은 AI 베이스). 단순한 전투 두뇌 담당.
	AIControllerClass = ACBRogueController::StaticClass();
}

TSoftObjectPtr<UCBAILoadout> ACBRogueCharacter::GetAILoadout() const
{
	return RogueLoadout;
}

UCBCombatComponent* ACBRogueCharacter::GetCBCombatComponent() const
{
	return RogueCombatComponent.Get();
}
