// project
#include "Characters/CBOutlawCharacter.h"
#include "DataAssets/Loadout/CBOutlawLoadout.h"
#include "Components/Combat/CBOutlawCombatComponent.h"
#include "Controllers/CBOutlawController.h"

ACBOutlawCharacter::ACBOutlawCharacter()
{
	// Combat Component 생성 (보스/엘리트 전용). ASC·AttributeSet·이동/빙의 설정은 AI 베이스에서 처리.
	OutlawCombatComponent = CreateDefaultSubobject<UCBOutlawCombatComponent>(TEXT("CBOutlawCombatComponent"));

	// 빙의할 AI 컨트롤러 지정 (자동 빙의 설정은 AI 베이스). 복잡한 전투 두뇌 담당.
	AIControllerClass = ACBOutlawController::StaticClass();
}

TSoftObjectPtr<UCBAILoadout> ACBOutlawCharacter::GetAILoadout() const
{
	return OutlawLoadout;
}

UCBCombatComponent* ACBOutlawCharacter::GetCBCombatComponent() const
{
	return OutlawCombatComponent.Get();
}
