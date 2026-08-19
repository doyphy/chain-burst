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

	UE_LOG(LogTemp, Log, TEXT("[Session] 서버 접속 시도: %s"), *TargetAddress);

	// TRAVEL_Absolute: 현재 맵을 기준으로 삼지 않고 이 주소로 새로 접속함
	LocalPlayerController->ClientTravel(TargetAddress, TRAVEL_Absolute);
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

	Local_HandleConnectionFailure(FailureReason);
}

// [로컬] 맵 이동 실패 델리게이트 콜백 함수
void UCBSessionSubsystem::Local_HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type InFailureType, const FString& InErrorString)
{
	UE_LOG(LogTemp, Warning, TEXT("[Session] 맵 이동 실패: %s (%s)"), ETravelFailure::ToString(InFailureType), *InErrorString);

	// 내 월드의 실패만 처리함
	if (InWorld && InWorld != GetWorld()) return;

	Local_HandleConnectionFailure(LOCTEXT("TravelFailure", "맵을 여는 데 실패했습니다."));
}

// [로컬] 실패 사유 방송 + 메인 메뉴 복귀
void UCBSessionSubsystem::Local_HandleConnectionFailure(const FText& InFailureReason)
{
	// 위젯이 모달 등으로 표시하도록 사유를 알림.
	OnConnectionFailed.Broadcast(InFailureReason);

	// 메인 메뉴는 ini 의 GameDefaultMap.
	const FString MainMenuMap = UGameMapsSettings::GetGameDefaultMap();
	if (MainMenuMap.IsEmpty()) return;

	// 현재 레벨 가져오기.
	const UWorld* CurrentWorld = GetWorld();
	if (CurrentWorld)
	{
		// 이미 메인 메뉴에 있으면 다시 열지 않음.
		const FString CurrentMap = UWorld::RemovePIEPrefix(CurrentWorld->GetOutermost()->GetName());
		if (CurrentMap == MainMenuMap) return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Session] 메인 메뉴로 복귀: %s"), *MainMenuMap);

	// 메인 메뉴 레벨 열기.
	UGameplayStatics::OpenLevel(this, FName(*MainMenuMap));
}

#undef LOCTEXT_NAMESPACE
