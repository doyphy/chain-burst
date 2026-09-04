// project
#include "GameStates/CBGameStateBase.h"

// engine
#include "GameFramework/PlayerState.h"

// 플레이어가 목록에 들어올 때. PlayerState 가 자기 PostInitializeComponents 에서 스스로 호출함 (서버·클라 공통)
void ACBGameStateBase::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);

	// 목록 변경을 방송. 받는 쪽이 PlayerArray 를 다시 읽으므로 누가 들어왔는지는 전달하지 않음
	OnPlayerListChanged.Broadcast();
}

// 플레이어가 목록에서 빠질 때 (퇴장·PlayerState 파괴)
void ACBGameStateBase::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);

	OnPlayerListChanged.Broadcast();
}
