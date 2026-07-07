// project
#include "Characters/CBRogueCharacter.h"
#include "DataAssets/Loadout/CBRogueLoadout.h"

TSoftObjectPtr<UCBCharacterLoadout> ACBRogueCharacter::GetAILoadout() const
{
	return RogueLoadout;
}
