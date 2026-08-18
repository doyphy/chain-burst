#pragma once

#include "CoreMinimal.h"
#include "GameModes/CBGameModeBase.h"
#include "CBLobbyGameMode.generated.h"

/**
 * 로비 레벨의 게임모드.
 * 모두 준비되면 게임플레이 레벨로 이동시킴.
 */
UCLASS()
class CHAINBURST_API ACBLobbyGameMode : public ACBGameModeBase
{
	GENERATED_BODY()
};
