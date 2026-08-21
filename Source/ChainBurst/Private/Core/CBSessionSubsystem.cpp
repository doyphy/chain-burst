// project
#include "Core/CBSessionSubsystem.h"

// engine
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
#include "Engine/PendingNetGame.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Online/Auth.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineServices.h"
#include "Online/Sessions.h"
#include "SocketSubsystem.h"

using namespace UE::Online;

#define LOCTEXT_NAMESPACE "CBSession"

namespace
{
	// 참가 주소를 비워서 호출했을 때 쓸 기본값.
	const TCHAR* CB_DefaultJoinAddress = TEXT("127.0.0.1");
}

// 로컬 세션 이름. 한 번에 하나만 유지하므로 상수 하나로 충분함
const FName UCBSessionSubsystem::SessionName = TEXT("ChainBurstSession");
const FName UCBSessionSubsystem::SessionSchemaName = TEXT("ChainBurstSessionSchema");

// 호스트 주소·방 이름을 실어 보내는 세션 속성 키.
// 제공자마다 주소를 얻는 방법이 달라, 키 하나로 통일해 조회 코드를 하나로 유지함
const FName UCBSessionSubsystem::HostAddressSettingKey = TEXT("CB_HostAddress");
const FName UCBSessionSubsystem::DisplayNameSettingKey = TEXT("CB_DisplayName");

// 현재 인원을 실어 보내는 세션 속성 키.
const FName UCBSessionSubsystem::CurrentPlayersSettingKey = TEXT("CB_CurrentPlayers");

// 서버 정원을 넘기는 URL 옵션 이름. AGameSession::InitOptions 가 이 이름으로 읽음(엔진 규약이라 바꿀 수 없음)
const FString UCBSessionSubsystem::MaxPlayersUrlOption = TEXT("MaxPlayers");

// 진행 중인 매치의 거부 문자열. 엔진의 "Server full." 과 같은 자리(PreLogin 의 ErrorMessage)에 실려 클라이언트로 옴
const FString UCBSessionSubsystem::MatchInProgressError = TEXT("Match in progress.");

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
	// 만들어 둔 세션을 정리함.
	// 에디터는 종료 전까지 살아있기에 여기서 안 지우면 다음 실행의 검색에 지난 세션이 계속 잡힘
	Local_LeaveActiveSession();

	if (GEngine)
	{
		// 접속 실패 델리게이트 구독 해제.
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}

	Super::Deinitialize();
}

#pragma region Session

// [로컬] 세션 인터페이스 조회
ISessionsPtr UCBSessionSubsystem::Local_ResolveSessionsInterface() const
{
	const UGameInstance* OwningGameInstance = GetGameInstance();
	if (!OwningGameInstance) return nullptr;

	// PIE로 테스트 시 한 프로세스에 게임 인스턴스가 여럿이라, 월드 컨텍스트 이름으로 서비스 인스턴스를 구분해야 함.
	const FWorldContext* WorldContext = OwningGameInstance->GetWorldContext();
	const FName InstanceName = WorldContext ? WorldContext->ContextHandle : NAME_None;

	// 해당 게임 인스턴스의 온라인 서비스 가져오기. 제공자(Null/EOS)는 ini 의 [OnlineServices] DefaultServices 가 정함
	const IOnlineServicesPtr Services = GetServices(EOnlineServices::Default, InstanceName);
	if (!Services)
	{
		UE_LOG(LogTemp, Error, TEXT("[Session] 온라인 서비스를 찾을 수 없음. [OnlineServices] DefaultServices 설정과 플러그인 활성화를 확인할 것"));
		return nullptr;
	}

	// 온라인 서비스의 세션 인터페이스 가져오기
	return Services->GetSessionsInterface();
}

// [로컬] 로컬 플레이어의 계정 ID 조회
FAccountId UCBSessionSubsystem::Local_ResolveLocalAccountId() const
{
	const UGameInstance* OwningGameInstance = GetGameInstance();
	if (!OwningGameInstance) return FAccountId();

	const FWorldContext* WorldContext = OwningGameInstance->GetWorldContext();
	const FName InstanceName = WorldContext ? WorldContext->ContextHandle : NAME_None;

	// 온라인 서비스 가져오기
	const IOnlineServicesPtr Services = GetServices(EOnlineServices::Default, InstanceName);
	if (!Services) return FAccountId();

	// 온라인 서비스의 인증 인터페이스 가져오기
	const IAuthPtr Auth = Services->GetAuthInterface();
	if (!Auth) return FAccountId();

	// 첫 번째 로컬 플레이어 기준. 분할 화면은 아직 고려하지 않음
	const ULocalPlayer* LocalPlayer = OwningGameInstance->GetFirstGamePlayer();
	if (!LocalPlayer) return FAccountId();

	// 로컬 플레이어의 플랫폼 계정 ID 조회. Null 제공자는 로그인 없이 바로 얻어짐. EOS 는 로그인 후에야 유효해짐
	const TOnlineResult<FAuthGetLocalOnlineUserByPlatformUserId> Result =
		Auth->GetLocalOnlineUserByPlatformUserId({ LocalPlayer->GetPlatformUserId() });

	if (!Result.IsOk())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Session] 로컬 계정을 얻지 못함: %s"), *Result.GetErrorValue().GetLogString());
		return FAccountId();
	}

	// 로컬 계정 ID 반환
	return Result.GetOkValue().AccountInfo->AccountId;
}

// [로컬][호스트] 자기 PC의 IP를 문자열로 만들어 반환함 (호스트가 세션에 실어 참가자에게 알려주는 용도)
FString UCBSessionSubsystem::Local_ResolveHostAddress() const
{
	// 플랫폼의 소켓 서브시스템 가져오기
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem) return FString();
	
	// 라우팅 조회로 주소를 확정하지 못해 아래 주소가 어댑터 목록의 첫 값(추정)으로 대체됐는지 여부.
	// true 면 신뢰도가 낮지만, 지금은 노출용 주소 하나만 필요해 참고하지 않음
	bool bCanBindAll = false;
	
	// 이 PC 로컬 주소 가져오기.
	const TSharedPtr<FInternetAddr> LocalAddr = SocketSubsystem->GetLocalHostAddr(*GLog, bCanBindAll);
	if (!LocalAddr.IsValid()) return FString();

	// 포트는 붙이지 않음. 참가 측이 기본 포트로 접속하며, 필요해지면 여기서 함께 실어 보냄
	return LocalAddr->ToString(false);
}

// [로컬] 세션 생성 후 로비 열기
bool UCBSessionSubsystem::Local_CreateAndHostSession(TSoftObjectPtr<UWorld> InLobbyLevel, const FString& InSessionDisplayName, int32 InMaxPlayers /* = 4 */)
{
	if (InLobbyLevel.IsNull())
	{
		Local_HandleSessionFailure(LOCTEXT("CreateFailedNoLevel", "로비 레벨이 지정되지 않았습니다."), TEXT("로비 레벨 미지정"));
		return false;
	}

	// 세션 인터페이스와 로컬 계정 ID 조회. 없으면 세션 작업을 할 수 없음
	const ISessionsPtr Sessions = Local_ResolveSessionsInterface();
	const FAccountId LocalAccountId = Local_ResolveLocalAccountId();
	
	if (!Sessions || !LocalAccountId.IsValid())
	{
		Local_HandleSessionFailure(LOCTEXT("CreateFailedNoService", "온라인 서비스를 사용할 수 없습니다."), TEXT("세션 인터페이스/계정 없음"));
		return false;
	}

	// 콜백에서 열어야 하므로 로비 레벨과 정원을 따로 보관.
	PendingLobbyLevel = InLobbyLevel;
	PendingMaxPlayers = FMath::Max(1, InMaxPlayers);

	// 세션 생성 요청 파라미터 구성
	FCreateSession::Params Params;
	Params.SessionSettings.SchemaName = SessionSchemaName;
	Params.LocalAccountId = LocalAccountId;
	Params.SessionName = SessionName;
	Params.bIsLANSession = bUseLANSessions;
	Params.SessionSettings.NumMaxConnections = PendingMaxPlayers;
	Params.SessionSettings.JoinPolicy = ESessionJoinPolicy::Public;


	// 호스트 주소를 세션에 실어 보냄. 참가 측은 이 값을 그대로 접속 주소로 씀
	Params.SessionSettings.CustomSettings.Emplace(
		HostAddressSettingKey,
		FCustomSessionSetting{ FSchemaVariant(Local_ResolveHostAddress()), ESchemaAttributeVisibility::Public });

	// 목록에 보일 방 이름
	const FString DisplayName = InSessionDisplayName.TrimStartAndEnd().IsEmpty()
		? LOCTEXT("DefaultSessionName", "ChainBurst 방").ToString()
		: InSessionDisplayName.TrimStartAndEnd();

	// 방 이름을 세션에 실어 보냄. 참가 측은 이 값을 그대로 표시용으로 씀
	Params.SessionSettings.CustomSettings.Emplace(
		DisplayNameSettingKey,
		FCustomSessionSetting{ FSchemaVariant(DisplayName), ESchemaAttributeVisibility::Public });

	// 현재 인원을 1(호스트)로 두고 시작함.
	Params.SessionSettings.CustomSettings.Emplace(
		CurrentPlayersSettingKey,
		FCustomSessionSetting{ FSchemaVariant(static_cast<int64>(1)), ESchemaAttributeVisibility::Public });

	UE_LOG(LogTemp, Log, TEXT("[Session] 세션 생성 요청: %s (LAN=%d, 최대 %d명)"), *DisplayName, bUseLANSessions ? 1 : 0, Params.SessionSettings.NumMaxConnections);

	// 세션 생성 요청. 완료 콜백에서 로비를 엶
	Sessions->CreateSession(MoveTemp(Params))
		.OnComplete(this, &UCBSessionSubsystem::Local_HandleCreateSessionComplete);

	return true;
}

// [로컬] 세션 생성 완료 콜백
void UCBSessionSubsystem::Local_HandleCreateSessionComplete(const TOnlineResult<FCreateSession>& InResult)
{
	if (!InResult.IsOk())
	{
		Local_HandleSessionFailure(LOCTEXT("CreateFailed", "방을 만들지 못했습니다."), InResult.GetErrorValue().GetLogString());
		return;
	}
	
	// 세션에 들어가 있음을 표시
	bHasActiveSession = true;
	// 자신이 세션의 호스트임을 표시
	bIsSessionHost = true;
	// 종료 시점에는 다시 조회할 수 없으므로 지금 보관해 둠
	ActiveSessionAccountId = Local_ResolveLocalAccountId();

	UE_LOG(LogTemp, Log, TEXT("[Session] 세션 생성 성공. 로비를 엶"));

	// 세션 생성이 끝나면 로비를 엶.
	Local_HostLobby(PendingLobbyLevel, PendingMaxPlayers);
}

// [서버] 광고 중인 세션 설정 갱신
void UCBSessionSubsystem::Auth_UpdateAdvertisedSession(FSessionSettingsUpdate&& InMutations)
{
	// 세션을 들고 있는 호스트가 아니면 무시.
	if (!bHasActiveSession || !bIsSessionHost) return;

	// 세션 인터페이스와 활성 세션 계정 ID 조회. 없으면 세션 작업을 할 수 없음
	const ISessionsPtr Sessions = Local_ResolveSessionsInterface();
	if (!Sessions || !ActiveSessionAccountId.IsValid()) return;

	// 갱신 요청 파라미터 구성
	FUpdateSessionSettings::Params Params;
	Params.LocalAccountId = ActiveSessionAccountId;
	Params.SessionName = SessionName;
	Params.Mutations = MoveTemp(InMutations);

	// 갱신 요청.
	Sessions->UpdateSessionSettings(MoveTemp(Params))
		.OnComplete([](const TOnlineResult<FUpdateSessionSettings>& InResult)
		{
			if (!InResult.IsOk())
			{
				UE_LOG(LogTemp, Warning, TEXT("[Session] 세션 광고 갱신 실패: %s"), *InResult.GetErrorValue().GetLogString());
			}
		});
}

// [서버] 광고 중인 세션의 현재 인원 갱신 (ACBLobbyGameMode에서 호출)
void UCBSessionSubsystem::Auth_UpdateAdvertisedPlayerCount(int32 InCurrentPlayers)
{
	// 범위 제한.
	const int32 ClampedPlayers = FMath::Clamp(InCurrentPlayers, 0, FMath::Max(1, PendingMaxPlayers));

	// 세션 설정 갱신 요청 구성. CustomSettings 에 현재 인원을 실어 보냄
	FSessionSettingsUpdate Mutations;
	Mutations.UpdatedCustomSettings.Emplace(
		CurrentPlayersSettingKey,
		FCustomSessionSetting{ FSchemaVariant(static_cast<int64>(ClampedPlayers)), ESchemaAttributeVisibility::Public });

	// 세션 설정 갱신
	Auth_UpdateAdvertisedSession(MoveTemp(Mutations));
}

// [서버] 광고 중인 세션의 참가 허용 여부 변경 (ACBLobbyGameMode에서 호출)
void UCBSessionSubsystem::Auth_SetSessionAcceptingPlayers(bool bInAllowNewMembers)
{
	FSessionSettingsUpdate Mutations;
	Mutations.bAllowNewMembers = bInAllowNewMembers;

	// 세션 설정 갱신
	Auth_UpdateAdvertisedSession(MoveTemp(Mutations));
}

// [로컬] 세션 검색
bool UCBSessionSubsystem::Local_FindSessions(int32 InMaxResults /* = 20 */)
{
	// 세션 인터페이스와 로컬 계정 ID 조회. 없으면 세션 작업을 할 수 없음
	const ISessionsPtr Sessions = Local_ResolveSessionsInterface();
	const FAccountId LocalAccountId = Local_ResolveLocalAccountId();
	if (!Sessions || !LocalAccountId.IsValid())
	{
		Local_HandleSessionFailure(LOCTEXT("FindFailedNoService", "온라인 서비스를 사용할 수 없습니다."), TEXT("세션 인터페이스/계정 없음"));
		return false;
	}

	// 검색 요청 파라미터 구성
	FFindSessions::Params Params;
	Params.LocalAccountId = LocalAccountId;
	Params.MaxResults = static_cast<uint32>(FMath::Max(1, InMaxResults));
	Params.bFindLANSessions = bUseLANSessions;

	UE_LOG(LogTemp, Log, TEXT("[Session] 세션 검색 요청 (LAN=%d)"), bUseLANSessions ? 1 : 0);

	// 검색 요청. 완료 콜백에서 결과를 표시용 목록으로 옮기고 방송함
	Sessions->FindSessions(MoveTemp(Params))
		.OnComplete(this, &UCBSessionSubsystem::Local_HandleFindSessionsComplete);

	return true;
}

// [로컬] 세션 검색 완료 콜백
void UCBSessionSubsystem::Local_HandleFindSessionsComplete(const TOnlineResult<FFindSessions>& InResult)
{
	// 이전 결과는 인덱스가 어긋나지 않도록 항상 비우고 시작함
	FoundSessions.Reset();
	FoundSessionIds.Reset();

	// 검색 실패 시 방송만 하고 종료. 검색 실패 사유는 로그로 남김
	if (!InResult.IsOk())
	{
		Local_HandleSessionFailure(LOCTEXT("FindFailed", "방을 찾지 못했습니다."), InResult.GetErrorValue().GetLogString());
		OnSessionSearchCompleted.Broadcast(false, 0);
		return;
	}

	// 세션 인터페이스 가져오기.
	const ISessionsPtr Sessions = Local_ResolveSessionsInterface();
	if (!Sessions)
	{
		OnSessionSearchCompleted.Broadcast(false, 0);
		return;
	}

	// 같은 세션이 여러 번 검색될 수 있기에 TSet 으로 중복 제거.
	TSet<FOnlineSessionId> SeenSessionIds;

	// 검색 결과 순회. (검색 결과가 많으면 여기서 시간이 걸릴 수 있음)
	for (const FOnlineSessionId& SessionId : InResult.GetOkValue().FoundSessionIds)
	{
		// 이미 담은 방이면 건너뜀
		bool bAlreadySeen = false;
		SeenSessionIds.Add(SessionId, &bAlreadySeen);
		if (bAlreadySeen) continue;

		// 검색은 ID 만 돌려주므로, 표시에 필요한 값은 세션을 하나씩 조회해 옮겨 담음
		const TOnlineResult<FGetSessionById> SessionResult = Sessions->GetSessionById({ SessionId });
		if (!SessionResult.IsOk()) continue;

		// 세션 정보 가져오기
		const TSharedRef<const ISession>& Session = SessionResult.GetOkValue().Session;

		// 검색 결과 목록에 추가. 인덱스는 FoundSessionIds 와 대응됨
		FoundSessions.Emplace(Local_BuildSearchEntry(*Session));
		FoundSessionIds.Emplace(SessionId);
	}

	UE_LOG(LogTemp, Log, TEXT("[Session] 세션 검색 완료: %d개"), FoundSessions.Num());

	// 검색 결과 방송. UI 위젯이 구독해 표시를 갱신함
	OnSessionSearchCompleted.Broadcast(true, FoundSessions.Num());
}

// [로컬] 세션에서 표시·참가 판정에 쓸 값을 읽어 담음
FCBSessionSearchEntry UCBSessionSubsystem::Local_BuildSearchEntry(const ISession& InSession) const
{
	// 세션 설정 가져오기.
	const FSessionSettings& Settings = InSession.GetSessionSettings();

	FCBSessionSearchEntry Entry;
	Entry.MaxPlayers = static_cast<int32>(Settings.NumMaxConnections);

	// 세션 정보에서 현재 인원을 가져오기. (키 값 : CurrentPlayersSettingKey)
	const FCustomSessionSetting* CurrentPlayersSetting = Settings.CustomSettings.Find(CurrentPlayersSettingKey);

	// 현재 인원 데이터 유호성 검사
	if (CurrentPlayersSetting && CurrentPlayersSetting->Data.GetType() == ESchemaAttributeType::Int64)
	{
		// 현재 인원 값을 가져와 Entry.CurrentPlayers 에 저장
		Entry.CurrentPlayers = static_cast<int32>(CurrentPlayersSetting->Data.GetInt64());
	}

	// 인원을 광고하지 않는 방(구버전 호스트 등)은 알 수 없음을 뜻하는 0 으로 두되, 정원 판정에서 막지는 않음
	Entry.CurrentPlayers = FMath::Clamp(Entry.CurrentPlayers, 0, Entry.MaxPlayers);

	// 정원이 찼으면 참가 불가. IsJoinable() 만으로는 정원 판정이 되지 않아 인원을 함께 봄
	Entry.bIsJoinable = InSession.IsJoinable() && Entry.CurrentPlayers < Entry.MaxPlayers;

	// 방 이름은 호스트가 실어 보낸 속성. 없으면 대체 문구
	if (const FCustomSessionSetting* DisplayNameSetting = Settings.CustomSettings.Find(DisplayNameSettingKey))
	{
		Entry.DisplayName = DisplayNameSetting->Data.GetString();
	}
	if (Entry.DisplayName.IsEmpty())
	{
		Entry.DisplayName = LOCTEXT("UnnamedSession", "이름 없는 방").ToString();
	}

	return Entry;
}

// [로컬] 검색 결과의 세션에 참가
bool UCBSessionSubsystem::Local_JoinFoundSession(int32 InSearchResultIndex)
{
	if (!FoundSessionIds.IsValidIndex(InSearchResultIndex))
	{
		Local_HandleSessionFailure(LOCTEXT("JoinFailedBadIndex", "선택한 방을 찾을 수 없습니다."),
			FString::Printf(TEXT("잘못된 검색 결과 인덱스: %d"), InSearchResultIndex));
		return false;
	}

	// 세션 인터페이스와 로컬 계정 ID 조회. 없으면 세션 작업을 할 수 없음
	const ISessionsPtr Sessions = Local_ResolveSessionsInterface();
	const FAccountId LocalAccountId = Local_ResolveLocalAccountId();
	if (!Sessions || !LocalAccountId.IsValid())
	{
		Local_HandleSessionFailure(LOCTEXT("JoinFailedNoService", "온라인 서비스를 사용할 수 없습니다."), TEXT("세션 인터페이스/계정 없음"));
		return false;
	}

	// 검색 결과 인덱스에 대응하는 세션 ID 조회
	const FOnlineSessionId TargetSessionId = FoundSessionIds[InSearchResultIndex];

	// 참가 직전에 방 상태를 다시 읽음. (갱신 전 정보를 읽기 때문에, 그 사이 시작된 방은 걸러내지 못함)
	const TOnlineResult<FGetSessionById> SessionResult = Sessions->GetSessionById({ TargetSessionId });
	if (!SessionResult.IsOk())
	{
		Local_HandleSessionFailure(LOCTEXT("JoinFailedSessionGone", "방 정보가 만료되었습니다. 목록을 새로고침해 주세요."),
			FString::Printf(TEXT("세션 조회 실패: %s"), *SessionResult.GetErrorValue().GetLogString()));
		return false;
	}

	// 다시 읽은 값으로 표시용 항목을 갱신함.
	FCBSessionSearchEntry& Entry = FoundSessions[InSearchResultIndex];
	Entry = Local_BuildSearchEntry(*SessionResult.GetOkValue().Session);

	// 참가할 수 없는 방이면 여기서 멈춤
	if (!Entry.bIsJoinable)
	{
		Local_HandleSessionFailure(LOCTEXT("JoinFailedNotJoinable", "참가할 수 없는 방입니다. 목록을 새로고침해 주세요."),
			FString::Printf(TEXT("참가 불가한 방: %s (%d/%d)"), *Entry.DisplayName, Entry.CurrentPlayers, Entry.MaxPlayers));
		return false;
	}

	// 참가 요청 파라미터 구성
	FJoinSession::Params Params;
	Params.LocalAccountId = LocalAccountId;
	Params.SessionName = SessionName;
	Params.SessionId = TargetSessionId;

	UE_LOG(LogTemp, Log, TEXT("[Session] 세션 참가 요청: %s (%d/%d)"), *Entry.DisplayName, Entry.CurrentPlayers, Entry.MaxPlayers);

	// 참가 요청. 완료 콜백에서 호스트 주소로 접속함
	Sessions->JoinSession(MoveTemp(Params))
		.OnComplete(this, &UCBSessionSubsystem::Local_HandleJoinSessionComplete);

	return true;
}

// [로컬] 세션 참가 완료 콜백
void UCBSessionSubsystem::Local_HandleJoinSessionComplete(const TOnlineResult<FJoinSession>& InResult)
{
	if (!InResult.IsOk())
	{
		Local_HandleSessionFailure(LOCTEXT("JoinFailed", "방에 참가하지 못했습니다."), InResult.GetErrorValue().GetLogString());
		return;
	}

	// 세션에 들어가 있음을 표시.
	bHasActiveSession = true;
	// 세션의 호스트는 아니므로 false.
	bIsSessionHost = false;
	// 종료 시점에는 다시 조회할 수 없으므로 지금 보관해 둠
	ActiveSessionAccountId = Local_ResolveLocalAccountId();

	// 세션 인터페이스 가져오기.
	const ISessionsPtr Sessions = Local_ResolveSessionsInterface();
	if (!Sessions)
	{
		Local_HandleSessionFailure(LOCTEXT("JoinFailedNoAddress", "방의 주소를 얻지 못했습니다."), TEXT("세션 인터페이스 없음"));
		return;
	}

	// 참가한 세션의 세션 정보를 가져오기.
	const TOnlineResult<FGetSessionByName> SessionResult = Sessions->GetSessionByName({ SessionName });
	if (!SessionResult.IsOk())
	{
		Local_HandleSessionFailure(LOCTEXT("JoinFailedNoAddress", "방의 주소를 얻지 못했습니다."), SessionResult.GetErrorValue().GetLogString());
		return;
	}

	// 세션 정보에서 호스트 주소를 얻음. 없으면 실패 처리
	const FCustomSessionSetting* HostAddressSetting =
		SessionResult.GetOkValue().Session->GetSessionSettings().CustomSettings.Find(HostAddressSettingKey);

	// 호스트 주소 문자열로 변환.
	const FString HostAddress = HostAddressSetting ? HostAddressSetting->Data.GetString() : FString();
	if (HostAddress.IsEmpty())
	{
		Local_HandleSessionFailure(LOCTEXT("JoinFailedNoAddress", "방의 주소를 얻지 못했습니다."), TEXT("호스트 주소 속성이 비어 있음"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Session] 세션 참가 성공. 호스트로 접속: %s"), *HostAddress);

	// 호스트 주소로 접속. 실패 시 Local_HandleNetworkFailure() 에서 처리됨
	Local_JoinServerByAddress(HostAddress);
}

// [로컬] 활성 세션에서 빠져나옴 (호스트=파괴 / 참가자=떠나기)
void UCBSessionSubsystem::Local_LeaveActiveSession()
{
	// 세션에 들어가 있지 않으면 할 일 없음
	if (!bHasActiveSession) return;

	if (const ISessionsPtr Sessions = Local_ResolveSessionsInterface())
	{
		// 세션에 들어갈 때 보관해 둔 계정 ID 를 씀.
		// 게임 인스턴스 종료 중에는 로컬 플레이어가 이미 제거돼 다시 조회할 수 없음
		if (ActiveSessionAccountId.IsValid())
		{
			FLeaveSession::Params LeaveParams;
			LeaveParams.LocalAccountId = ActiveSessionAccountId;
			LeaveParams.SessionName = SessionName;

			// 호스트가 나가면 방 자체가 없어져야 하므로 파괴, 참가자는 떠나기만 함
			LeaveParams.bDestroySession = bIsSessionHost;

			UE_LOG(LogTemp, Log, TEXT("[Session] 세션에서 나감 (파괴=%d)"), bIsSessionHost ? 1 : 0);

			// 세션 떠나기 요청. 완료를 기다리지 않음
			Sessions->LeaveSession(MoveTemp(LeaveParams));
		}
	}

	bHasActiveSession = false;
	bIsSessionHost = false;
	ActiveSessionAccountId = FAccountId();
}

// [로컬] 세션 작업 실패 처리 (로그 + 사유 방송, 레벨 이동 없음)
void UCBSessionSubsystem::Local_HandleSessionFailure(const FText& InFailureReason, const FString& InLogContext)
{
	UE_LOG(LogTemp, Warning, TEXT("[Session] 세션 작업 실패: %s (%s)"), *InFailureReason.ToString(), *InLogContext);

	OnSessionOperationFailed.Broadcast(InFailureReason);
}

#pragma endregion

// [로컬] 로비 레벨을 리슨 서버로 엶
bool UCBSessionSubsystem::Local_HostLobby(TSoftObjectPtr<UWorld> InLobbyLevel, int32 InMaxPlayers /* = 0 */)
{
	if (InLobbyLevel.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("[Session] 호스트 실패: 로비 레벨이 지정되지 않음"));
		OnConnectionFailed.Broadcast(LOCTEXT("HostFailedNoLevel", "로비 레벨이 지정되지 않았습니다."));
		return false;
	}

	// 새 세션을 시작하므로 실패 사유 표시를 다시 켬
	bLeaveRequested = false;

	// listen: 리슨 서버로 염. 빠지면 조용히 단독 실행이 됨
	FString Options = TEXT("listen");

	// MaxPlayers: 레벨과 함께 스폰되는 AGameSession 이 InitOptions 에서 읽어 정원 판정에 씀.
	// 세션 광고의 인원과 같은 값에서 나와야 표시와 실제 정원이 어긋나지 않음. 0 이면 엔진 기본값을 그대로 둠
	if (InMaxPlayers > 0)
	{
		Options += FString::Printf(TEXT("?%s=%d"), *MaxPlayersUrlOption, InMaxPlayers);
	}

	UE_LOG(LogTemp, Log, TEXT("[Session] 리슨 서버로 로비 열기: %s (옵션: %s)"), *InLobbyLevel.ToString(), *Options);

	// 리슨 서버로 레벨 열기
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, InLobbyLevel, true, Options);
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

	// 세션에 들어가 있으면 먼저 빠져나옴
	Local_LeaveActiveSession();

	UE_LOG(LogTemp, Log, TEXT("[Session] 세션에서 나가 메인 메뉴로 이동: %s"), *MainMenuMap);

	// 클라이언트는 커넥션을 정리하며 나가야 하므로 로컬 플레이어 컨트롤러의 ClientTravel 을 씀.
	// 서버가 곧바로 Logout 을 받아 로비 인원 집계가 갱신됨
	if (CurrentWorld->GetNetMode() == NM_Client)
	{
		UGameInstance* OwningGameInstance = GetGameInstance();
		if (APlayerController* LocalPlayerController = OwningGameInstance ? OwningGameInstance->GetFirstLocalPlayerController() : nullptr)
		{
			// 로컬 플레이어 컨트롤러가 있으면 ClientTravel 로 접속을 정리하며 나감
			LocalPlayerController->ClientTravel(MainMenuMap, TRAVEL_Absolute);
			return true;
		}

		UE_LOG(LogTemp, Warning, TEXT("[Session] 나가기: 로컬 플레이어 컨트롤러가 없어 레벨을 직접 엶"));
	}

	// 호스트·단독 실행은 붙어 있는 서버가 없으므로 레벨을 직접 엶.
	// 호스트가 나가면 접속해 있던 클라이언트는 커넥션이 끊겨 각자 실패 경로로 메뉴에 돌아감
	UGameplayStatics::OpenLevel(this, FName(*MainMenuMap));
	return true;
}

// [로컬][PIE테스트전용] 방송된 실패가 이 게임 인스턴스의 것인지 판별
bool UCBSessionSubsystem::Local_IsOwnFailure(const UWorld* InFailureWorld, const UNetDriver* InFailureNetDriver) const
{
	// 월드가 실려 왔으면 내 월드인지로 판별하면 됨
	if (InFailureWorld)
	{
		return InFailureWorld == GetWorld();
	}

	// 월드가 없는 실패는 맵 로드 전(PendingNetGame) 단계.
	const UGameInstance* OwningGameInstance = GetGameInstance();
	const FWorldContext* WorldContext = OwningGameInstance ? OwningGameInstance->GetWorldContext() : nullptr;
	// 내 WorldContext에 PendingNetGame이 없으면 접속 시도 중이 아니므로 실패가 아님
	const UPendingNetGame* PendingGame = WorldContext ? WorldContext->PendingNetGame : nullptr;
	if (!PendingGame) return false;

	// 넷 드라이버가 함께 왔으면 같은 것인지까지 대조함.
	// 두 인스턴스가 동시에 접속을 시도해도 이 대조로 구분됨
	return !InFailureNetDriver || InFailureNetDriver == PendingGame->NetDriver;
}

// [로컬] 접속 실패 시 현재 레벨을 유지해야 하는지 (UCBOnlineSession 이 물어봄)
bool UCBSessionSubsystem::Local_ShouldKeepCurrentLevel(const UNetDriver* InNetDriver) const
{
	// 맵 로드 전 접속 시도만 현재 레벨을 유지함.
	return InNetDriver && InNetDriver->NetDriverName == NAME_PendingNetDriver;
}

// [로컬] 접속 실패 델리게이트 콜백 함수 (접속 실패·연결 끊김)
void UCBSessionSubsystem::Local_HandleNetworkFailure(UWorld* InWorld, UNetDriver* InNetDriver, ENetworkFailure::Type InFailureType, const FString& InErrorString)
{
	UE_LOG(LogTemp, Warning, TEXT("[Session] 네트워크 실패: %s (%s)"), ENetworkFailure::ToString(InFailureType), *InErrorString);

	// 남의 인스턴스에서 난 실패는 무시함 (한 프로세스에 PIE 창이 여럿일 때)
	if (!Local_IsOwnFailure(InWorld, InNetDriver)) return;

	// 호스트가 방을 열지 못한 경우.
	const bool bIsHostSetupFailure =
		InFailureType == ENetworkFailure::NetDriverCreateFailure ||
		InFailureType == ENetworkFailure::NetDriverListenFailure ||
		InFailureType == ENetworkFailure::NetDriverAlreadyExists;

	// 클라이언트가 접속 중 실패한 경우.
	const bool bIsClientFailure = InNetDriver && InNetDriver->GetNetMode() == NM_Client;
	
	// 클라이언트 접속 실패와 호스트 방 열기 실패만 처리함. 그 외는 무시 (필요하면 그때 추가함)
	if (!bIsClientFailure && !bIsHostSetupFailure) return;

	FText FailureReason;
	switch (InFailureType)
	{
	case ENetworkFailure::ConnectionLost:
	case ENetworkFailure::ConnectionTimeout:
		FailureReason = LOCTEXT("NetFailureDisconnected", "서버와의 연결이 끊어졌습니다.");
		break;

	// 리슨 서버를 열지 못한 경우. 포트 점유나 드라이버 설정 문제라 접속과는 원인이 다름
	case ENetworkFailure::NetDriverCreateFailure:
	case ENetworkFailure::NetDriverListenFailure:
	case ENetworkFailure::NetDriverAlreadyExists:
		FailureReason = LOCTEXT("NetFailureHostSetup", "방을 열지 못했습니다.");
		break;

	// 서로 다른 빌드로 붙은 경우
	case ENetworkFailure::OutdatedClient:
	case ENetworkFailure::OutdatedServer:
		FailureReason = LOCTEXT("NetFailureVersionMismatch", "게임 버전이 서로 다릅니다.");
		break;

	// 서버가 접속을 거부한 경우. AGameSession::ApproveLogin 이 반환한 문자열이 그대로 실려 옴.
	// 접속 단계에서 거부되면 PendingConnectionFailure, 접속 후 끊기면 FailureReceived 로 오므로 둘 다 확인함
	case ENetworkFailure::PendingConnectionFailure:
	case ENetworkFailure::FailureReceived:
		if (InErrorString.Contains(TEXT("Server full")))
		{
			FailureReason = LOCTEXT("NetFailureServerFull", "방이 가득 찼습니다.");
		}
		// 이미 시작된 매치.
		else if (InErrorString.Contains(MatchInProgressError))
		{
			FailureReason = LOCTEXT("NetFailureMatchInProgress", "이미 시작된 게임입니다.");
		}
		else
		{
			FailureReason = LOCTEXT("NetFailureCannotConnect", "서버에 접속하지 못했습니다.");
		}
		break;

	default:
		FailureReason = LOCTEXT("NetFailureUnknown", "네트워크 오류로 접속이 종료되었습니다.");
		break;
	}

	// 실패 사유를 알림. 접속 시도 단계면 현재 레벨에 머문 채 방송만 하고, 그 밖에는 메인 메뉴로 되돌림
	Local_HandleConnectionFailure(FailureReason, Local_ShouldKeepCurrentLevel(InNetDriver));
}

// [로컬] 맵 이동 실패 델리게이트 콜백 함수
void UCBSessionSubsystem::Local_HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type InFailureType, const FString& InErrorString)
{
	UE_LOG(LogTemp, Warning, TEXT("[Session] 맵 이동 실패: %s (%s)"), ETravelFailure::ToString(InFailureType), *InErrorString);

	// 남의 인스턴스에서 난 실패는 무시함.
	// 이동 실패는 넷 드라이버 인자가 없어 월드 외에 거를 수단이 없음.
	if (!Local_IsOwnFailure(InWorld, nullptr)) return;

	// 메인 메뉴로 돌아가도록 처리함. 실패 사유는 경고창으로 표시됨
	Local_HandleConnectionFailure(LOCTEXT("TravelFailure", "맵을 여는 데 실패했습니다."));
}

// [로컬] 실패 사유 알림 + 세션 이탈 + (필요하면) 메인 메뉴 복귀
void UCBSessionSubsystem::Local_HandleConnectionFailure(const FText& InFailureReason, bool bKeepCurrentLevel /* = false */)
{
	// 스스로 나가는 중이면 사유를 알리지 않음. 자기가 닫은 커넥션이 경고창으로 돌아오면 안 됨
	if (!bLeaveRequested)
	{
		// 레벨을 넘겨야 하는 실패만 사유를 보관함.
		if (!bKeepCurrentLevel)
		{
			PendingFailureReason = InFailureReason;
		}
		OnConnectionFailed.Broadcast(InFailureReason);
	}

	// 접속이 끊긴 이상 그 세션에 남아 있을 이유가 없음. 호스트면 파괴하고, 참가자면 떠나기만 함.
	// 정리해 두지 않으면 다시 방을 만들거나 참가할 때 이전 세션이 남아 걸림
	Local_LeaveActiveSession();

	// 현재 레벨을 유지하는 실패는 이동하지 않음. 엔진의 기본 맵 복귀는 UCBOnlineSession 이 막아 둠
	if (bKeepCurrentLevel) return;

	// 이동할 메인 메뉴 맵. 이미 메인 메뉴면 비어 있음
	const FString MainMenuMap = Local_ResolveMainMenuTravelMap();
	if (MainMenuMap.IsEmpty()) return;

	UE_LOG(LogTemp, Log, TEXT("[Session] 메인 메뉴로 복귀: %s"), *MainMenuMap);

	// 메인 메뉴 레벨 열기.
	UGameplayStatics::OpenLevel(this, FName(*MainMenuMap));
}

// [로컬] 보관해 둔 실패 사유를 꺼내고 비움 (메인 메뉴 위젯이 호출)
bool UCBSessionSubsystem::Local_ConsumePendingFailureReason(FText& OutFailureReason)
{
	if (PendingFailureReason.IsEmpty()) return false;

	OutFailureReason = PendingFailureReason;

	// 한 번만 표시되도록 비움. 다음에 정상적으로 메인 메뉴에 들어왔을 때 옛 사유가 다시 뜨면 안 됨
	PendingFailureReason = FText::GetEmpty();

	return true;
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
