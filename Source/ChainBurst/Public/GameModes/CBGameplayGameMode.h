#pragma once

#include "CoreMinimal.h"
#include "GameModes/CBGameModeBase.h"
#include "CBGameplayGameMode.generated.h"

/**
 * 게임플레이 레벨의 게임모드.
 * 실제 매치 규칙(승패 판정·리스폰 등)을 담당함.
 * 진행 중인 매치로의 난입은 접속 승인 단계에서 거부함.
 */
UCLASS()
class CHAINBURST_API ACBGameplayGameMode : public ACBGameModeBase
{
	GENERATED_BODY()

protected:
	//~ Begin AGameModeBase Interface.
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	//~ End AGameModeBase Interface.

	/** 진행 중인 매치에 새 플레이어를 받을지. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Gameplay")
	bool bAllowJoinInProgress = false;
};
