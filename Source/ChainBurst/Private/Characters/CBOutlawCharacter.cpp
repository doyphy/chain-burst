// project
#include "Characters/CBOutlawCharacter.h"
#include "DataAssets/Loadout/CBOutlawLoadout.h"
#include "Components/Combat/CBOutlawCombatComponent.h"

ACBOutlawCharacter::ACBOutlawCharacter()
{
	// Combat Component 생성 (보스/엘리트 전용). ASC·AttributeSet·이동/빙의 설정은 AI 베이스에서 처리.
	OutlawCombatComponent = CreateDefaultSubobject<UCBOutlawCombatComponent>(TEXT("CBOutlawCombatComponent"));
}

TSoftObjectPtr<UCBCharacterLoadout> ACBOutlawCharacter::GetAILoadout() const
{
	return OutlawLoadout;
}

UCBCombatComponent* ACBOutlawCharacter::GetCBCombatComponent() const
{
	return OutlawCombatComponent.Get();
}
