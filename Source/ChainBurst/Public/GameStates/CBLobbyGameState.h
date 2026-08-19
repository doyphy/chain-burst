#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Types/CBDelegates.h"
#include "CBLobbyGameState.generated.h"

/**
 * 로비 레벨의 게임 스테이트.
 * 준비 인원 집계를 들고 전 클라이언트에 복제함.
 */
UCLASS()
class CHAINBURST_API ACBLobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	/** 준비 인원이 바뀌었음을 알리는 델리게이트. (로비 위젯에서 구독) */
	UPROPERTY(BlueprintAssignable, Category = "ChainBurst|Lobby")
	FCBOnLobbyReadyStateChanged OnReadyStateChanged;

	/**
	 * 게임 시작을 알리는 델리게이트.
	 * 맵을 넘어가기 직전에 울리며, 서버·클라이언트 모두 같은 시점에 받음.
	 * 게임 시작 전 처리 작업을 할 수 있도록 전원에게 신호를 보냄
	 */
	UPROPERTY(BlueprintAssignable, Category = "ChainBurst|Lobby")
	FCBOnMatchStarting OnMatchStarting;

	/** [서버 → 전원] 게임 시작 델리게이트 방송. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_NotifyMatchStarting();

	/**
	 * [서버] 집계 결과를 반영하고 방송함. (서버에서만 실행)
	 * @param InReadyCount 준비를 마친 인원
	 * @param InTotalCount 로비 전체 인원
	 */
	void Auth_SetReadyState(int32 InReadyCount, int32 InTotalCount);

	/**
	 * 현재 값으로 신호를 다시 방송함.
	 * PlayerState 와 GameState 의 복제 도착 순서가 보장되지 않으므로, 늦게 도착한 쪽에서도 호출해 위젯을 맞춤.
	 */
	void NotifyReadyStateChanged();

	/** [Getter] 준비를 마친 인원 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Lobby")
	FORCEINLINE int32 GetReadyCount() const { return ReadyCount; }

	/** [Getter] 로비 전체 인원 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Lobby")
	FORCEINLINE int32 GetTotalCount() const { return TotalCount; }

	/** 전원이 준비를 마쳤는지 여부 (아무도 없으면 false). 호스트 위젯이 시작 버튼 전환에 사용함. */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Lobby")
	bool IsAllReady() const;

protected:
	//~ Begin AActor Interface.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor Interface.

	/** 집계 값이 바뀌었을 때 호출되는 콜백. */
	UFUNCTION()
	void OnRep_ReadyState();

	/** 준비를 마친 인원 */
	UPROPERTY(ReplicatedUsing = OnRep_ReadyState)
	int32 ReadyCount = 0;

	/** 로비 전체 인원 */
	UPROPERTY(ReplicatedUsing = OnRep_ReadyState)
	int32 TotalCount = 0;
};
