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
 * 플레이어가 죽으면 일정 시간 뒤 PlayerStart 에 다시 스폰시킴 (지연·지점 규칙을 이 클래스가 소유).
 * AI 처치 경험치를 전 플레이어 공유로 쌓고, 레벨이 오르면 월드를 정지한 채 플레이어마다 카드를 뽑아 주고 고른 카드의 효과를 적용함.
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
	 * @param InController 죽은 플레이어의 컨트롤러
	 */
	void Auth_HandlePlayerDeath(AController* InController);

protected:
	/**
	 * [서버] 실제로 다시 스폰시키는 함수. (예약 타이머가 만료되면 호출)
	 * ASC 가 PlayerState 소유라 폰을 갈아도 살아남으므로, 스폰 전에 사망 상태와 이전 로드아웃 부여분을 걷어냄.
	 * @param InController 다시 스폰할 플레이어의 컨트롤러
	 */
	void Auth_RespawnPlayer(AController* InController);

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
};
