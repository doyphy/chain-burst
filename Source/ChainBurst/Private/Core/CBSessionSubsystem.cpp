// project
#include "Core/CBSessionSubsystem.h"

// engine
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "CBSession"

namespace
{
	// 참가 주소를 비워서 호출했을 때 쓸 기본값.
	const TCHAR* CB_DefaultJoinAddress = TEXT("127.0.0.1");
}

// 서브시스템 생성 시 호출됨
void UCBSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!GEngine) return;

	// 접속 실패 델리게이트 구독.
	NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UCBSessionSubsystem::Local_HandleNetworkFailure);
	TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UCBSessionSubsystem::Local_HandleTravelFailure);
}

// 서브시스템 해제 시 호출됨
void UCBSessionSubsystem::Deinitialize()
{
	if (GEngine)
	{
		// 접속 실패 델리게이트 구독 해제.
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}

	Super::Deinitialize();
}

// [로컬] 로비 레벨을 리슨 서버로 엶
bool UCBSessionSubsystem::Local_HostLobby(TSoftObjectPtr<UWorld> InLobbyLevel)
{
	if (InLobbyLevel.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("[Session] 호스트 실패: 로비 레벨이 지정되지 않음"));
		OnConnectionFailed.Broadcast(LOCTEXT("HostFailedNoLevel", "로비 레벨이 지정되지 않았습니다."));
		return false;
	}

	// 새 세션을 시작하므로 실패 사유 표시를 다시 켬
	bLeaveRequested = false;

	UE_LOG(LogTemp, Log, TEXT("[Session] 리슨 서버로 로비 열기: %s"), *InLobbyLevel.ToString());

	// 리슨 서버로 레벨 열기
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, InLobbyLevel, true, TEXT("listen"));
	return true;
}

// [로컬] 주소로 서버에 접속함
bool UCBSessionSubsystem::Local_JoinServerByAddress(const FString& InAddress)
{
	UGameInstance* OwningGameInstance = GetGameInstance();
	APlayerController* LocalPlayerController = OwningGameInstance ? OwningGameInstance->GetFirstLocalPlayerController() : nullptr;
	if (!LocalPlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("[Session] 참가 실패: 로컬 플레이어 컨트롤러가 없음"));
		OnConnectionFailed.Broadcast(LOCTEXT("JoinFailedNoController", "접속을 시작할 수 없습니다."));
		return false;
	}

	// 위젯의 입력 그대로 들어오므로 공백을 걷어내고, 비어 있으면 로컬 테스트 기본값으로 대체함
	FString TargetAddress = InAddress.TrimStartAndEnd();
	if (TargetAddress.IsEmpty())
	{
		TargetAddress = CB_DefaultJoinAddress;
	}

	// 새 세션을 시작하므로 실패 사유 표시를 다시 켬
	bLeaveRequested = false;

	UE_LOG(LogTemp, Log, TEXT("[Session] 서버 접속 시도: %s"), *TargetAddress);

	// TRAVEL_Absolute: 현재 맵을 기준으로 삼지 않고 이 주소로 새로 접속함
	LocalPlayerController->ClientTravel(TargetAddress, TRAVEL_Absolute);
	return true;
}

// [로컬] 세션에서 빠져나와 메인 메뉴 레벨로 돌아감
bool UCBSessionSubsystem::Local_LeaveToMainMenu()
{
	const FString MainMenuMap = Local_ResolveMainMenuTravelMap();

	// 설정이 비었거나 이미 메인 메뉴라 이동할 필요가 없음
	if (MainMenuMap.IsEmpty()) return false;

	const UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld) return false;

	// 나가는 과정에서 올라오는 접속 끊김을 경고로 표시하지 않도록 표시해 둠
	bLeaveRequested = true;

	UE_LOG(LogTemp, Log, TEXT("[Session] 세션에서 나가 메인 메뉴로 이동: %s"), *MainMenuMap);

	// 클라이언트는 커넥션을 정리하며 나가야 하므로 로컬 플레이어 컨트롤러의 ClientTravel 을 씀.
	// 서버가 곧바로 Logout 을 받아 로비 인원 집계가 갱신됨
	if (CurrentWorld->GetNetMode() == NM_Client)
	{
		UGameInstance* OwningGameInstance = GetGameInstance();
		if (APlayerController* LocalPlayerController = OwningGameInstance ? OwningGameInstance->GetFirstLocalPlayerController() : nullptr)
		{
			LocalPlayerController->ClientTravel(MainMenuMap, TRAVEL_Absolute);
			return true;
		}

		// 컨트롤러가 없으면 아래 레벨 열기로 넘어감 (메뉴에는 도착해야 함)
		UE_LOG(LogTemp, Warning, TEXT("[Session] 나가기: 로컬 플레이어 컨트롤러가 없어 레벨을 직접 엶"));
	}

	// 호스트·단독 실행은 붙어 있는 서버가 없으므로 레벨을 직접 엶.
	// 호스트가 나가면 접속해 있던 클라이언트는 커넥션이 끊겨 각자 실패 경로로 메뉴에 돌아감
	UGameplayStatics::OpenLevel(this, FName(*MainMenuMap));
	return true;
}

// [로컬] 접속 실패 델리게이트 콜백 함수 (접속 실패·연결 끊김)
void UCBSessionSubsystem::Local_HandleNetworkFailure(UWorld* InWorld, UNetDriver* InNetDriver, ENetworkFailure::Type InFailureType, const FString& InErrorString)
{
	UE_LOG(LogTemp, Warning, TEXT("[Session] 네트워크 실패: %s (%s)"), ENetworkFailure::ToString(InFailureType), *InErrorString);

	// 남의 월드에서 난 실패는 무시함.
	if (InWorld && InWorld != GetWorld()) return;

	// 호스트는 클라이언트가 나가도 로비에 남아 있어야 하므로, 클라이언트 쪽 실패만 처리함.
	// 넷 드라이버가 없으면(접속 시도 전 단계) 우리가 접속하려던 쪽이므로 클라이언트로 취급함
	const bool bIsClientFailure = !InNetDriver || InNetDriver->GetNetMode() == NM_Client;
	if (!bIsClientFailure) return;

	FText FailureReason;
	switch (InFailureType)
	{
	case ENetworkFailure::ConnectionLost:
	case ENetworkFailure::ConnectionTimeout:
		FailureReason = LOCTEXT("NetFailureDisconnected", "서버와의 연결이 끊어졌습니다.");
		break;

	case ENetworkFailure::PendingConnectionFailure:
		FailureReason = LOCTEXT("NetFailureCannotConnect", "서버에 접속하지 못했습니다.");
		break;

	default:
		FailureReason = LOCTEXT("NetFailureUnknown", "네트워크 오류로 접속이 종료되었습니다.");
		break;
	}

	// 메인 메뉴로 돌아가도록 처리함. 실패 사유는 경고창으로 표시됨
	Local_HandleConnectionFailure(FailureReason);
}

// [로컬] 맵 이동 실패 델리게이트 콜백 함수
void UCBSessionSubsystem::Local_HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type InFailureType, const FString& InErrorString)
{
	UE_LOG(LogTemp, Warning, TEXT("[Session] 맵 이동 실패: %s (%s)"), ETravelFailure::ToString(InFailureType), *InErrorString);

	// 내 월드의 실패만 처리함
	if (InWorld && InWorld != GetWorld()) return;

	// 메인 메뉴로 돌아가도록 처리함. 실패 사유는 경고창으로 표시됨
	Local_HandleConnectionFailure(LOCTEXT("TravelFailure", "맵을 여는 데 실패했습니다."));
}

// [로컬] 실패 사유 방송 + 메인 메뉴 복귀
void UCBSessionSubsystem::Local_HandleConnectionFailure(const FText& InFailureReason)
{
	// 스스로 나가는 중이면 사유를 알리지 않음.
	if (!bLeaveRequested)
	{
		// 위젯이 모달 등으로 표시하도록 사유를 알림.
		OnConnectionFailed.Broadcast(InFailureReason);
	}

	// 이동할 메인 메뉴 맵. 이미 메인 메뉴면 비어 있음
	const FString MainMenuMap = Local_ResolveMainMenuTravelMap();
	if (MainMenuMap.IsEmpty()) return;

	UE_LOG(LogTemp, Log, TEXT("[Session] 메인 메뉴로 복귀: %s"), *MainMenuMap);

	// 메인 메뉴 레벨 열기.
	UGameplayStatics::OpenLevel(this, FName(*MainMenuMap));
}

// [로컬] 이동할 메인 메뉴 맵 이름을 반환 (이동이 필요 없으면 빈 문자열)
FString UCBSessionSubsystem::Local_ResolveMainMenuTravelMap() const
{
	// 메인 메뉴는 ini 의 GameDefaultMap.
	const FString MainMenuMap = UGameMapsSettings::GetGameDefaultMap();
	if (MainMenuMap.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[Session] 메인 메뉴 맵(GameDefaultMap)이 지정되지 않음"));
		return FString();
	}

	// 현재 레벨 가져오기.
	const UWorld* CurrentWorld = GetWorld();
	if (CurrentWorld)
	{
		// 이미 메인 메뉴에 있으면 다시 열지 않음. PIE 는 패키지 이름에 인스턴스 접두사가 붙으므로 걷어내고 비교함
		const FString CurrentMap = UWorld::RemovePIEPrefix(CurrentWorld->GetOutermost()->GetName());
		if (CurrentMap == MainMenuMap) return FString();
	}

	return MainMenuMap;
}

#undef LOCTEXT_NAMESPACE
