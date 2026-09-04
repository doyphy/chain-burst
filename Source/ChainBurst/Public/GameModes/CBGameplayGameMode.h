#pragma once

#include "CoreMinimal.h"
#include "GameModes/CBGameModeBase.h"
#include "Engine/TimerHandle.h"
#include "CBGameplayGameMode.generated.h"

/**
 * 게임플레이 레벨의 게임모드.
 * 실제 매치 규칙(승패 판정·리스폰 등)을 담당함.
 * 진행 중인 매치로의 난입은 접속 승인 단계에서 거부함.
 * 플레이어가 죽으면 일정 시간 뒤 PlayerStart 에 다시 스폰시킴 (지연·지점 규칙을 이 클래스가 소유).
 */
UCLASS()
class CHAINBURST_API ACBGameplayGameMode : public ACBGameModeBase
{
	GENERATED_BODY()

public:
	/**
	 * [서버] 플레이어가 죽었을 때 호출. RespawnDelay 뒤에 다시 스폰하도록 예약함. (서버에서만 실행)
	 * 이미 예약된 컨트롤러면 아무것도 하지 않음.
	 * @param InController 죽은 플레이어의 컨트롤러
	 */
	void Auth_HandlePlayerDeath(AController* InController);

protected:
	//~ Begin AGameModeBase Interface.
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	/** [서버] 나간 플레이어의 리스폰 예약을 취소함. */
	virtual void Logout(AController* Exiting) override;
	//~ End AGameModeBase Interface.

	/**
	 * [서버] 실제로 다시 스폰시키는 함수. (예약 타이머가 만료되면 호출)
	 * ASC 가 PlayerState 소유라 폰을 갈아도 살아남으므로, 스폰 전에 사망 상태와 이전 로드아웃 부여분을 걷어냄.
	 * @param InController 다시 스폰할 플레이어의 컨트롤러
	 */
	void Auth_RespawnPlayer(AController* InController);

	/**
	 * [서버] 다시 스폰하기 전에 ASC 를 사망 이전 상태로 되돌리는 함수.
	 * 사망 GE(Status.Dead 부여)를 제거하고 이전 폰의 로드아웃 부여분을 회수함.
	 * 스탯은 새 폰이 로드아웃의 StartupEffects 를 다시 적용하면서 원복됨.
	 * @param InController 정리 대상 플레이어의 컨트롤러
	 */
	void Auth_ClearDeathState(const AController* InController) const;

	/** 사망 후 다시 스폰하기까지의 지연(초) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Gameplay", meta = (ClampMin = "0.0"))
	float RespawnDelay = 5.f;

	/** 진행 중인 매치에 새 플레이어를 받을지. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Gameplay")
	bool bAllowJoinInProgress = false;

private:
	/**
	 * 컨트롤러별 리스폰 예약 타이머.
	 * 죽은 폰이 아니라 게임모드가 들고 있어야, 시체가 먼저 파괴되어도 리스폰이 그대로 진행됨.
	 */
	TMap<TWeakObjectPtr<AController>, FTimerHandle> RespawnTimers;
};
