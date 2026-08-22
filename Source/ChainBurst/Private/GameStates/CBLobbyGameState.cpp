// project
#include "GameStates/CBLobbyGameState.h"
#include "AssetManager/CBAssetManager.h"
#include "Core/CBGameInstance.h"
#include "DataAssets/Character/CBCharacterCatalog.h"

// engine
#include "Net/UnrealNetwork.h"

void ACBLobbyGameState::BeginPlay()
{
	Super::BeginPlay();

	// 고를 수 있는 캐릭터 클래스를 미리 로드
	PreloadCharacterClasses();
}

// [전 인스턴스] 고를 수 있는 캐릭터 클래스를 미리 로드
void ACBLobbyGameState::PreloadCharacterClasses()
{
	// 캐릭터 카탈로그 가져오기
	const UCBGameInstance* CBGameInstance = GetGameInstance<UCBGameInstance>();
	const UCBCharacterCatalog* CharacterCatalog = CBGameInstance ? CBGameInstance->GetCharacterCatalog() : nullptr;

	// 카탈로그를 등록하지 않았으면 미리 로드할 것도 없음
	if (!CharacterCatalog) return;

	// 등록된 캐릭터 클래스 경로를 모아 한 번에 비동기 로드.
	TArray<FSoftObjectPath> CharacterClassPaths;
	CharacterCatalog->GetCharacterClassPaths(CharacterClassPaths);

	if (CharacterClassPaths.IsEmpty()) return;

	const int32 PreloadCount = CharacterClassPaths.Num();
	
	// 비동기 로드
	UCBAssetManager::Get().LoadAssetsAsync(CharacterClassPaths, [PreloadCount]()
	{
		UE_LOG(LogTemp, Log, TEXT("[Lobby] 캐릭터 클래스 %d개 미리 로드 완료"), PreloadCount);
	});
}

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
