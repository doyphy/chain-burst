#pragma once

#include "CoreMinimal.h"
#include "GameModes/CBGameModeBase.h"
#include "CBGameplayGameMode.generated.h"

/**
 * 게임플레이 레벨의 게임모드.
 * 실제 매치 규칙(승패 판정·리스폰 등)을 담당함.
 */
UCLASS()
class CHAINBURST_API ACBGameplayGameMode : public ACBGameModeBase
{
	GENERATED_BODY()
};
