// project
#include "GameModes/CBLobbyGameMode.h"
#include "Core/CBSessionSubsystem.h"
#include "GameStates/CBLobbyGameState.h"
#include "PlayerState/CBPlayerState.h"

// engine
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "Misc/PackageName.h"

ACBLobbyGameMode::ACBLobbyGameMode()
{
	// 로비 게임모드의 게임 스테이트는 ACBLobbyGameState 로 설정
	GameStateClass = ACBLobbyGameState::StaticClass();
}

void ACBLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// 전체 인원이 늘었으므로 다시 집계
	Auth_RefreshReadyState();
}

void ACBLobbyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	// 나가는 플레이어의 PlayerState 는 아직 목록에 남아 있으므로 집계에서 빼고 셈.
	// 준비를 마친 사람이 나갔을 때 "전원 준비" 조건이 풀리려면 여기서 갱신해야 함
	Auth_RefreshReadyState(Exiting ? Exiting->PlayerState : nullptr);
}

// [로컬] 세션 서브시스템 조회 (세션 광고 갱신용)
UCBSessionSubsystem* ACBLobbyGameMode::Local_GetSessionSubsystem() const
{
	const UGameInstance* OwningGameInstance = GetGameInstance();
	return OwningGameInstance ? OwningGameInstance->GetSubsystem<UCBSessionSubsystem>() : nullptr;
}

// [서버] 준비 인원을 다시 세어 게임 스테이트에 반영 (서버에서 실행)
void ACBLobbyGameMode::Auth_RefreshReadyState(const APlayerState* InIgnorePlayerState /* = nullptr */)
{
	ACBLobbyGameState* LobbyGameState = GetGameState<ACBLobbyGameState>();
	if (!LobbyGameState) return;

	int32 ReadyCount = 0;
	int32 TotalCount = 0;

	// 전체 플레이어를 순회하며 준비 인원과 전체 인원을 다시 계산
	for (const APlayerState* PlayerState : LobbyGameState->PlayerArray)
	{
		// 나가는 플레이어의 PlayerState 는 아직 목록에 남아 있으므로 집계에서 제외
		if (!PlayerState || PlayerState == InIgnorePlayerState) continue;
		
		// 전체 인원 증가
		++TotalCount;

		// 준비 상태를 확인.
		const ACBPlayerState* CBPlayerState = Cast<ACBPlayerState>(PlayerState);
		if (CBPlayerState && CBPlayerState->IsReady())
		{
			// 준비 인원 증가
			++ReadyCount;
		}
	}

	// 집계 결과를 게임 스테이트에 반영. OnRep_ReadyState() 가 호출되어 전 클라이언트에 복제됨
	LobbyGameState->Auth_SetReadyState(ReadyCount, TotalCount);

	// 세션 광고에도 현재 인원을 갱신. 호스트가 아니면 아무것도 하지 않음
	if (UCBSessionSubsystem* SessionSubsystem = Local_GetSessionSubsystem())
	{
		SessionSubsystem->Auth_UpdateAdvertisedPlayerCount(TotalCount);
	}
}

// [서버] 호스트의 시작 요청 처리 (ACBChaserController::Server_RequestStartMatch 에서 호출됨)
void ACBLobbyGameMode::Auth_TryStartMatch(const APlayerController* InRequester)
{
	// 이미 이동 중이면 무시 (버튼 연타·중복 요청 방어)
	if (bTravelStarted) return;

	// 호스트가 아닌 곳에서 요청하면 무시 (Auth_TryStartMatch 함수는 서버에서만 실행함)
	if (!InRequester || !InRequester->IsLocalController())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Lobby] 호스트가 아닌 곳에서 게임 시작을 요청함: %s"), *GetNameSafe(InRequester));
		return;
	}

	const ACBLobbyGameState* LobbyGameState = GetGameState<ACBLobbyGameState>();
	if (!LobbyGameState) return;

	// 최소 인원 미달
	if (LobbyGameState->GetTotalCount() < MinPlayersToStart)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Lobby] 인원 부족으로 시작 거부: %d명 (최소 %d명)"),
			LobbyGameState->GetTotalCount(), MinPlayersToStart);
		return;
	}

	// 아직 준비하지 않은 사람이 있음
	if (!LobbyGameState->IsAllReady())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Lobby] 준비되지 않은 플레이어가 있어 시작 거부: %d/%d"),
			LobbyGameState->GetReadyCount(), LobbyGameState->GetTotalCount());
		return;
	}

	// 모든 조건을 통과했으므로 게임플레이 레벨로 이동
	Auth_TravelToGameplayLevel();
}

// [서버] 접속한 전원을 게임플레이 레벨로 이동시킴
void ACBLobbyGameMode::Auth_TravelToGameplayLevel()
{
	if (GameplayLevel.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("[Lobby] 게임플레이 레벨이 지정되지 않아 이동할 수 없음"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	bTravelStarted = true;

	// 매치가 시작되면 난입을 받지 않으므로 세션을 닫음.
	if (UCBSessionSubsystem* SessionSubsystem = Local_GetSessionSubsystem())
	{
		SessionSubsystem->Auth_SetSessionAcceptingPlayers(false);
	}

	// 소프트 참조의 경로는 "/Game/.../L_X.L_X" 형태라 패키지 이름만 뽑아 씀
	const FString LevelPath = FPackageName::ObjectPathToPackageName(GameplayLevel.ToString());

	UE_LOG(LogTemp, Log, TEXT("[Lobby] 게임플레이 레벨로 이동: %s"), *LevelPath);

	// 이동 직전에 전원에게 알림.
	// 위젯 제거 등 로컬에서 게임 시작 전 처리 작업을 할 수 있도록 전원에게 신호를 보냄
	if (ACBLobbyGameState* LobbyGameState = GetGameState<ACBLobbyGameState>())
	{
		// 전원에게 게임 시작 신호를 방송.
		LobbyGameState->Multicast_NotifyMatchStarting();
	}

	// ServerTravel 시 "?listen" 옵션을 붙여 호스트가 Listen 서버가 되도록 함
	FString TravelURL = LevelPath + TEXT("?listen");

	// AGameSession 이 레벨마다 새로 스폰되기 때문에 URL 다시 설정
	if (GameSession && GameSession->MaxPlayers > 0)
	{
		TravelURL += FString::Printf(TEXT("?MaxPlayers=%d"), GameSession->MaxPlayers);
	}

	World->ServerTravel(TravelURL);
}
