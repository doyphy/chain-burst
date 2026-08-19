#pragma once

#include "CoreMinimal.h"
#include "GameModes/CBGameModeBase.h"
#include "CBLobbyGameMode.generated.h"

class APlayerState;

/**
 * 로비 레벨의 게임모드.
 * 플레이어들의 준비 상태를 집계해 게임 스테이트에 반영하고, 호스트의 시작 요청을 검증해 게임플레이 레벨로 전원을 이동시킴.
 */
UCLASS()
class CHAINBURST_API ACBLobbyGameMode : public ACBGameModeBase
{
	GENERATED_BODY()

public:
	ACBLobbyGameMode();

	/**
	 * [서버] 준비 인원을 다시 세어 게임 스테이트에 반영함. (서버에서만 실행)
	 * @param InIgnorePlayerState 집계에서 제외할 PlayerState (나가는 중이라 아직 목록에 남아 있는 경우)
	 */
	void Auth_RefreshReadyState(const APlayerState* InIgnorePlayerState = nullptr);

	/**
	 * [서버] 게임 시작 요청을 검증하고 통과하면 게임플레이 레벨로 이동시킴. (서버에서만 실행)
	 * @param InRequester 시작을 요청한 컨트롤러
	 */
	void Auth_TryStartMatch(const APlayerController* InRequester);

protected:
	//~ Begin AGameModeBase Interface.
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	//~ End AGameModeBase Interface.

	/** [서버] 접속한 전원을 게임플레이 레벨로 이동시킴 (ServerTravel). */
	void Auth_TravelToGameplayLevel();

	/** 전원이 준비되면 이동할 게임플레이 레벨. 로비 게임모드 BP 에서 설정. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Lobby")
	TSoftObjectPtr<UWorld> GameplayLevel;

	/** 게임을 시작할 수 있는 최소 인원. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Lobby", meta = (ClampMin = "1"))
	int32 MinPlayersToStart = 1;

	/** 이미 이동을 시작했는지. 요청이 중복으로 들어와도 한 번만 수행하기 위함. */
	bool bTravelStarted = false;
};
