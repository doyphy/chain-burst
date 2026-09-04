#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Types/CBDelegates.h"
#include "CBGameStateBase.generated.h"

/**
 * 프로젝트 공용 게임 스테이트 베이스.
 * 접속한 플레이어 목록(PlayerArray)이 바뀌었음을 알리는 신호를 제공함.
 * 레벨별 규칙은 파생 클래스(로비 등)가 담당함.
 */
UCLASS()
class CHAINBURST_API ACBGameStateBase : public AGameStateBase
{
	GENERATED_BODY()

public:
	/**
	 * 접속한 플레이어 목록이 바뀌었음을 알리는 델리게이트. (플레이어 목록 UI 에서 구독)
	 * 누가 들어오고 나갔는지는 전달하지 않으므로, 받는 쪽은 PlayerArray 를 다시 읽어 갱신함.
	 */
	UPROPERTY(BlueprintAssignable, Category = "ChainBurst|Player")
	FCBOnPlayerListChanged OnPlayerListChanged;

	//~ Begin AGameStateBase Interface.
		/** 플레이어가 목록에 들어올 때 호출됨 (서버·클라 공통). 목록 변경을 방송함. */
	virtual void AddPlayerState(APlayerState* PlayerState) override;
		/** 플레이어가 목록에서 빠질 때 호출됨 (서버·클라 공통). 목록 변경을 방송함. */
	virtual void RemovePlayerState(APlayerState* PlayerState) override;
	//~ End AGameStateBase Interface.
};
