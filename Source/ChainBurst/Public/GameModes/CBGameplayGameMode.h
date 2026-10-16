#pragma once

#include "CoreMinimal.h"
#include "GameModes/CBGameModeBase.h"
#include "Containers/Ticker.h"
#include "Engine/TimerHandle.h"
#include "CBGameplayGameMode.generated.h"

class UCBLevelUpData;

/**
 * 게임플레이 레벨의 게임모드.
 * 실제 매치 규칙(승패 판정·리스폰·레벨업 등)을 담당함.
 * 진행 중인 매치로의 난입은 접속 승인 단계에서 거부함.
 * 플레이어가 죽으면 일정 시간 뒤 PlayerStart 에 다시 스폰시킴 (지연·지점 규칙을 이 클래스가 소유). 보스전 중 사망자는 보스 구역의 부활 지점에 스폰.
 * AI 처치 경험치를 전 플레이어 공유로 쌓고, 레벨이 오르면 월드를 정지한 채 플레이어마다 카드를 뽑아 주고 고른 카드의 효과를 적용함.
 * 보스를 처치하면 매치를 끝내고 보스 시체가 사라진 뒤 전원에게 클리어 화면을 띄우며, 호스트가 요청하면 세션을 유지한 채 전원을 로비로 데려감.
 */
UCLASS()
class CHAINBURST_API ACBGameplayGameMode : public ACBGameModeBase
{
	GENERATED_BODY()

public:
	ACBGameplayGameMode();

protected:
	//~ Begin AActor Interface.
	/** [서버] 레벨업 타임아웃 티커를 해제함 (월드 밖에서 도는 티커라 게임모드가 사라져도 남음). */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface.

	//~ Begin AGameModeBase Interface.
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	/** [서버] 나간 플레이어의 리스폰 예약을 취소하고, 레벨업 카드를 기다리는 중이었으면 대기 명단에서 뺌. */
	virtual void Logout(AController* Exiting) override;
	//~ End AGameModeBase Interface.

	/** 진행 중인 매치에 새 플레이어를 받을지. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Gameplay")
	bool bAllowJoinInProgress = false;

#pragma region Respawn
public:
	/**
	 * [서버] 플레이어가 죽었을 때 호출. RespawnDelay 뒤에 다시 스폰하도록 예약함. (서버에서만 실행)
	 * 이미 예약된 컨트롤러면 아무것도 하지 않음.
	 * 보스전 중에 죽었으면 부활 위치를 이 시점에 보스 구역의 부활 지점으로 정해 둠 (부활 시점엔 전멸 리셋으로 보스전이 끝나 있을 수 있음).
	 * @param InController 죽은 플레이어의 컨트롤러
	 */
	void Auth_HandlePlayerDeath(AController* InController);

	/** [Getter] 사망 후 다시 스폰하기까지의 지연(초). 플레이어 시체가 사라지는 시점이기도 함 (소멸 연출이 이 시점에 끝나도록 맞춤) */
	FORCEINLINE float GetRespawnDelay() const { return RespawnDelay; }

protected:
	/**
	 * [서버] 실제로 다시 스폰시키는 함수. (예약 타이머가 만료되면 호출)
	 * ASC 가 PlayerState 소유라 폰을 갈아도 살아남으므로, 스폰 전에 사망 상태와 이전 로드아웃 부여분을 걷어냄.
	 * @param InController 다시 스폰할 플레이어의 컨트롤러
	 * @param InRespawnTransform 정해 둔 부활 위치 (보스전 중 사망). 비어 있으면 엔진 기본 규칙으로 PlayerStart 에서 스폰
	 */
	void Auth_RespawnPlayer(AController* InController, const TOptional<FTransform>& InRespawnTransform);

	/**
	 * [서버] 보스전이 진행 중인 보스 구역이 있으면 그 부활 지점을 돌려주는 함수.
	 * 구역이 게임모드에 등록·해제하는 대신 사망 시점에 레벨을 훑어 물음 (등록 해제 누락이 생기지 않음).
	 * @param InController 부활시킬 플레이어의 컨트롤러 (폰 클래스를 정하는 데 씀)
	 * @return 부활 위치. 보스전 중이 아니면 비어 있음
	 */
	TOptional<FTransform> Auth_FindBossFightRespawnTransform(AController* InController);

	/**
	 * [서버] 다시 스폰하기 전에 ASC 를 사망 이전 상태로 되돌리는 함수.
	 * 사망 GE(Status.Dead 부여)를 제거하고 이전 폰의 로드아웃 부여분을 회수함.
	 * 스탯은 새 폰이 로드아웃의 StartupEffects 를 다시 적용하면서 원복됨. 레벨업 카드 효과는 로드아웃 부여분이 아니라 남음.
	 * @param InController 정리 대상 플레이어의 컨트롤러
	 */
	void Auth_ClearDeathState(const AController* InController) const;

	/** 사망 후 다시 스폰하기까지의 지연(초) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Gameplay", meta = (ClampMin = "0.0"))
	float RespawnDelay = 5.f;

private:
	/**
	 * 컨트롤러별 리스폰 예약 타이머.
	 * 죽은 폰이 아니라 게임모드가 들고 있어야, 시체가 먼저 파괴되어도 리스폰이 그대로 진행됨.
	 */
	TMap<TWeakObjectPtr<AController>, FTimerHandle> RespawnTimers;
#pragma endregion

#pragma region LevelUp
public:
	/**
	 * [서버] 전 플레이어가 공유하는 경험치를 더하는 함수. (AI 사망 통지 ACBAICharacter::Auth_OnDeath 가 호출)
	 * 필요량을 넘은 만큼 레벨을 올리고, 진행 중인 레벨업이 없으면 카드 선택을 시작함.
	 * 매치가 클리어된 뒤에는 무시함 (클리어 화면 위로 레벨업 정지가 걸리지 않게).
	 * @param InAmount 더할 경험치 (0 이하면 무시)
	 */
	void Auth_AddExperience(int32 InAmount);

	/**
	 * [서버] 플레이어의 카드 선택 처리. (ACBChaserController::Server_SelectLevelUpCard 가 호출)
	 * 이번 레벨업에서 카드를 받았고 아직 고르지 않은 플레이어의 유효한 슬롯만 받아들임 (타임아웃 뒤 늦게 온 선택은 무시).
	 * @param InPlayerController 고른 플레이어
	 * @param InSlot 제시받은 카드 중 몇 번째인지 (0부터)
	 */
	void Auth_SelectLevelUpCard(APlayerController* InPlayerController, int32 InSlot);

private:
	/**
	 * [서버] 레벨업 하나를 시작하는 함수.
	 * 플레이어마다 카드를 따로 뽑아 본인에게만 보내고, 월드를 정지하고, 선택 타임아웃을 검.
	 */
	void Auth_StartLevelUpRound();

	/**
	 * [서버] 한 플레이어의 선택을 확정하는 함수. 카드 효과를 적용하고 대기 명단에서 빼며, 마지막이면 레벨업을 끝냄.
	 * @param InPlayerController 고른 플레이어
	 * @param InCardIndex 적용할 카드 인덱스 (레벨업 데이터의 카드 배열 기준)
	 */
	void Auth_CompleteCardSelection(APlayerController* InPlayerController, int32 InCardIndex);

	/**
	 * [서버] 카드 효과를 플레이어 ASC 에 적용하는 함수.
	 * 로드아웃 부여 기록을 거치지 않으므로 무기 변경·사망 정리에 회수되지 않고 매치 끝까지 유지됨.
	 * @param InPlayerController 적용 대상 플레이어
	 * @param InCardIndex 적용할 카드 인덱스
	 */
	void Auth_ApplyLevelUpCard(const APlayerController* InPlayerController, int32 InCardIndex) const;

	/**
	 * [서버] 레벨업 하나를 끝내는 함수.
	 * 남은 레벨업이 있으면 정지를 유지한 채 다음 레벨업을 시작하고, 없으면 전원의 카드 창을 내리고 재개함.
	 */
	void Auth_FinishLevelUpRound();

	/**
	 * [서버] 선택 타임아웃 콜백. 아직 고르지 않은 플레이어마다 받은 카드 중 하나를 무작위로 적용하고 레벨업을 끝냄.
	 * 월드 정지 중에도 실제 시간으로 도는 코어 티커에서 호출됨.
	 * @return 다시 호출할지 (1회만 실행하므로 항상 false)
	 */
	bool HandleLevelUpTimeout(float DeltaTime);

	/**
	 * [서버] 레벨업 정지를 걸거나 푸는 함수.
	 * 반드시 PlayerController::SetPause 경로를 씀 — 게임모드의 SetPause 는 WorldSettings 즉시 송출이 빠져,
	 * 정지 직후 멈춘 서버 시간 때문에 정지 신호가 클라이언트로 나가지 않을 수 있음.
	 * @param bInPause true = 정지, false = 재개
	 */
	void Auth_SetLevelUpPause(bool bInPause);

	/** 정지 해제 조건. 레벨업이 남아 있는 동안은 다른 해제 요청(콘솔 pause 등)이 와도 풀리지 않음 */
	bool CanUnpauseForLevelUp() const;

	/** 게임 인스턴스에 등록된 레벨업 데이터. 없으면 nullptr */
	const UCBLevelUpData* GetLevelUpData() const;

	/** 아직 시작하지 않은 레벨업 수. 한 번에 여러 레벨이 오르면 하나씩 이어서 진행함 */
	int32 PendingLevelUps = 0;

	/** 레벨업(카드 선택)이 진행 중인지 */
	bool bLevelUpRoundActive = false;

	/** [서버 검증용] 이번 레벨업에서 아직 고르지 않은 플레이어별 제시 카드 인덱스. 비면 레벨업이 끝남 */
	TMap<TWeakObjectPtr<APlayerController>, TArray<int32>> PendingCardOffers;

	/** 선택 타임아웃 티커 핸들. 월드 타이머는 정지 중에 멈추므로 코어 티커를 씀 */
	FTSTicker::FDelegateHandle LevelUpTimeoutHandle;
#pragma endregion

#pragma region MatchEnd
public:
	/**
	 * [서버] 보스 처치 통지. (ACBBossArena 가 보스 사망을 확인하면 호출)
	 * 보스 처치 = 매치 클리어 확정. 클리어 시간을 기록하고, 이후 경험치를 받지 않음. 클리어 화면은 보스 시체가 사라진 뒤(Auth_HandleBossDespawned)에 띄움.
	 */
	void Auth_HandleBossDefeated();

	/**
	 * [서버] 처치한 보스가 사라졌다는 통지. (ACBBossArena 가 보스 파괴를 확인하면 호출)
	 * 플레이어마다 클리어 기록(클리어 시간 + 그 플레이어가 넣은 데미지)을 담아 클리어 화면을 띄움.
	 * 매치가 클리어되지 않았으면 무시함 (구역은 보스전마다 한 번만 부름).
	 */
	void Auth_HandleBossDespawned();

	/**
	 * [서버] 로비 복귀 요청 처리. (ACBChaserController::Server_RequestReturnToLobby 가 호출)
	 * 매치가 클리어됐고 요청자가 호스트일 때만, 세션을 유지한 채 접속한 전원을 데리고 로비로 이동함.
	 * 각자 세션을 나가 메인 메뉴로 가는 것은 이 경로가 아님 (ACBChaserController::Local_LeaveMatchToMainMenu).
	 * @param InRequester 요청한 컨트롤러
	 */
	void Auth_TryReturnToLobby(const APlayerController* InRequester);

protected:
	/** 매치가 끝나면 호스트가 전원을 데리고 돌아갈 로비 레벨. 게임플레이 게임모드 BP 에서 설정 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Gameplay")
	TSoftObjectPtr<UWorld> LobbyLevel;

private:
	/** 보스를 처치해 매치가 끝났는지. 이후 경험치를 받지 않고, 로비 복귀를 허용함 */
	bool bMatchCleared = false;

	/**
	 * 레벨 시작부터 보스 처치까지의 시간(초). 처치 순간의 월드 시간 - 게임플레이 월드는 레벨이 시작될 때 0 부터 흐르고,
	 * 레벨업 카드를 고르는 동안(월드 정지)은 멈추므로 그 시간이 빠짐
	 */
	float MatchClearTime = 0.f;

	/** 로비로 이동을 시작했는지. 버튼 연타 등 중복 요청이 와도 한 번만 이동하기 위함 */
	bool bReturnToLobbyStarted = false;
#pragma endregion
};
