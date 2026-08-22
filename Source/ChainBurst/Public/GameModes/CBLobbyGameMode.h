#pragma once

#include "CoreMinimal.h"
#include "GameModes/CBGameModeBase.h"
#include "CBLobbyGameMode.generated.h"

class APlayerState;
class UCBSessionSubsystem;

/**
 * 로비 레벨의 게임모드.
 * 플레이어들의 준비 상태를 집계해 게임 스테이트에 반영하고, 호스트의 시작 요청을 검증해 게임플레이 레벨로 전원을 이동시킴.
 * 집계한 인원은 세션 광고에도 실어 보내 방 목록의 현재 인원이 맞게 보이도록 하고, 매치가 시작되면 세션을 닫아 난입을 막음.
 * 캐릭터(무기) 변경 요청이 오면 같은 자리에서 폰을 다시 스폰해 고른 캐릭터로 갈아끼움.
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

	/**
	 * [서버] 고른 캐릭터로 폰을 다시 스폰함. (서버에서만 실행)
	 * @param InPlayer 다시 스폰할 플레이어의 컨트롤러
	 */
	void Auth_RespawnWithSelectedCharacter(APlayerController* InPlayer);

protected:
	//~ Begin AGameModeBase Interface.
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	/** [서버] 플레이어의 스폰 지점을 정함. 한 번 배정한 자리를 기억해 재스폰해도 같은 자리를 씀. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	//~ End AGameModeBase Interface.

	/** [서버] 접속한 전원을 게임플레이 레벨로 이동시킴 (ServerTravel). 이동 전에 세션을 닫아 난입을 막음. */
	void Auth_TravelToGameplayLevel();

	/** [로컬] 세션 광고를 갱신할 세션 서브시스템을 반환함. 없으면 nullptr. */
	UCBSessionSubsystem* Local_GetSessionSubsystem() const;

	/** 전원이 준비되면 이동할 게임플레이 레벨. 로비 게임모드 BP 에서 설정. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Lobby")
	TSoftObjectPtr<UWorld> GameplayLevel;

	/** 게임을 시작할 수 있는 최소 인원. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Lobby", meta = (ClampMin = "1"))
	int32 MinPlayersToStart = 1;

	/** 이미 이동을 시작했는지. 요청이 중복으로 들어와도 한 번만 수행하기 위함. */
	bool bTravelStarted = false;

	/**
	 * 컨트롤러별로 배정한 스폰 지점.
	 * 기억하지 않으면 캐릭터를 바꿀 때마다 다른 자리에 스폰되어 로비에서 자리가 뒤바뀜.
	 */
	TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<AActor>> AssignedPlayerStarts;
};
