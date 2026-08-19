// project
#include "GameStates/CBLobbyGameState.h"

// engine
#include "Net/UnrealNetwork.h"

void ACBLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 모든 플레이어의 위젯이 준비 인원을 표시하므로 전 클라이언트에 복제
	DOREPLIFETIME(ACBLobbyGameState, ReadyCount);
	DOREPLIFETIME(ACBLobbyGameState, TotalCount);
}

// [서버] 게임모드가 집계한 결과를 반영 (서버에서 실행)
void ACBLobbyGameState::Auth_SetReadyState(int32 InReadyCount, int32 InTotalCount)
{
	ReadyCount = InReadyCount;
	TotalCount = InTotalCount;

	// 서버에서는 OnRep 이 불리지 않으므로 직접 호출해 호스트 위젯에도 반영
	OnRep_ReadyState();
}

// [서버/클라이언트] 준비 인원 변경 신호 방송
void ACBLobbyGameState::NotifyReadyStateChanged()
{
	// 준비 인원 변경 신호를 방송. 위젯이 구독해 표시하도록 함.
	OnReadyStateChanged.Broadcast(ReadyCount, TotalCount);
}

// [전원] 게임 시작 신호를 각 인스턴스에서 방송 (위젯이 구독)
void ACBLobbyGameState::Multicast_NotifyMatchStarting_Implementation()
{
	OnMatchStarting.Broadcast();
}

bool ACBLobbyGameState::IsAllReady() const
{
	// 아무도 없는 로비를 "전원 준비"로 볼 수 없으므로 0명은 제외
	return TotalCount > 0 && ReadyCount >= TotalCount;
}

// TotalCount, ReadyCount 값이 바뀌었을 때 호출되는 콜백.
void ACBLobbyGameState::OnRep_ReadyState()
{
	NotifyReadyStateChanged();
}
