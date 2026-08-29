#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "Net/Core/Connection/NetEnums.h"
#include "Online/CoreOnline.h"
#include "Types/CBDelegates.h"
#include "Types/CBStructTypes.h"
#include "CBSessionSubsystem.generated.h"

namespace UE::Online
{
	class ISession;
	class ISessions;
	using ISessionsPtr = TSharedPtr<class ISessions>;

	struct FCreateSession;
	struct FFindSessions;
	struct FJoinSession;
	struct FLeaveSession;
	struct FSessionSettingsUpdate;

	template <typename OpType> class TOnlineResult;
}

/**
 * 멀티플레이 접속 창구 서브시스템.
 * 세션(방 생성·검색·참가·광고 갱신)과 실제 접속(레벨 열기·travel)을 모두 담당함.
 *
 * 실패는 두 갈래로 나눠 다룸 —
 *  · 접속 시도(맵 로드 전) 실패는 현재 레벨을 유지한 채 사유만 방송함. (엔진의 기본 맵 복귀는 UCBOnlineSession 이 막음)
 *  · 접속 후 끊김·맵 이동 실패는 사유를 보관하고 메인 메뉴로 되돌림.
 *
 * 맵을 넘어 살아남아야 접속 실패 신호를 받을 수 있으므로 게임 인스턴스 서브시스템으로 둠.
 */
UCLASS(Config = Engine)
class CHAINBURST_API UCBSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

#pragma region Core
	/** 서브시스템 수명 주기 */
public:
	/** 서브시스템 생성 시 호출됨. 접속 실패 델리게이트를 구독함. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 서브시스템 해제 시 호출됨. 구독을 해제함. */
	virtual void Deinitialize() override;
#pragma endregion

#pragma region Session
	/** 방 생성·검색·참가·나가기. 제공자(Null/EOS)는 ini 의 [OnlineServices] DefaultServices 가 정함. */
public:
	/**
	 * [로컬] 세션을 만들고, 성공하면 로비 레벨을 리슨 서버로 엶.
	 * @param InLobbyLevel 열 로비 레벨
	 * @param InSessionDisplayName 목록에 표시될 방 이름. 비우면 기본 문구를 씀
	 * @param InMaxPlayers 최대 인원
	 * @return 생성 요청을 보냈으면 true (성공 여부는 비동기로 결정됨)
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Session")
	bool Local_CreateAndHostSession(TSoftObjectPtr<UWorld> InLobbyLevel, const FString& InSessionDisplayName, int32 InMaxPlayers = 4);

	/**
	 * [로컬] 참가 가능한 세션을 검색함. 완료되면 OnSessionSearchCompleted 를 방송함.
	 * @param InMaxResults 최대 결과 수
	 * @return 검색 요청을 보냈으면 true
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Session")
	bool Local_FindSessions(int32 InMaxResults = 20);

	/**
	 * [로컬] 검색 결과의 세션에 참가하고, 성공하면 그 세션의 호스트 주소로 접속함.
	 * @param InSearchResultIndex GetFoundSessions() 배열의 인덱스
	 * @return 참가 요청을 보냈으면 true
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Session")
	bool Local_JoinFoundSession(int32 InSearchResultIndex);

	/**
	 * [서버] 광고 중인 세션의 현재 인원을 갱신함. 로비 게임모드가 인원 변화 시 호출함.
	 * 호스트가 아니거나 활성 세션이 없으면 아무것도 하지 않음
	 * @param InCurrentPlayers 광고할 현재 인원
	 */
	void Auth_UpdateAdvertisedPlayerCount(int32 InCurrentPlayers);

	/**
	 * [서버] 광고 중인 세션의 참가 허용 여부를 바꿈. 매치가 시작되면 false 로 닫아 목록에서 참가 불가로 보이게 함.
	 * 호스트가 아니거나 활성 세션이 없으면 아무것도 하지 않음
	 * @param bInAllowNewMembers 새 참가자를 받을지
	 */
	void Auth_SetSessionAcceptingPlayers(bool bInAllowNewMembers);

	/** [Getter] 마지막 검색 결과 (목록 위젯 표시용) */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Session")
	const TArray<FCBSessionSearchEntry>& GetFoundSessions() const { return FoundSessions; }

	/**
	 * 진행 중인 매치에 붙으려 할 때 서버가 돌려보내는 거부 문자열.
	 * 서버(ACBGameplayGameMode::PreLogin)가 실어 보내고 클라이언트(Local_HandleNetworkFailure)가 읽어 문구를 가름.
	 */
	static const FString MatchInProgressError;

	/** 세션 검색 완료 신호. 목록 위젯이 구독해 표시를 갱신함. */
	UPROPERTY(BlueprintAssignable, Category = "ChainBurst|Session")
	FCBOnSessionSearchCompleted OnSessionSearchCompleted;

	/** 세션 작업 실패 신호. 레벨 이동 없이 사유만 알림. */
	UPROPERTY(BlueprintAssignable, Category = "ChainBurst|Session")
	FCBOnSessionOperationFailed OnSessionOperationFailed;

private:
	/** [로컬] 세션 인터페이스를 반환함. 온라인 서비스가 없으면 nullptr. */
	UE::Online::ISessionsPtr Local_ResolveSessionsInterface() const;

	/**
	 * [로컬] 로컬 플레이어의 계정 ID 를 반환함. 유효하지 않으면 세션 작업을 할 수 없음.
	 * Null 제공자는 시작 시 계정을 자동 생성하므로 로그인 없이 바로 얻어짐.
	 * EOS 를 도입하면 로그인이 끝난 뒤에야 유효해지므로, 로그인 대기는 이 함수 앞에 붙임
	 */
	UE::Online::FAccountId Local_ResolveLocalAccountId() const;

	/** [로컬] 이 PC 가 리슨 서버로서 광고할 주소를 반환함. (제공자 무관하게 세션 속성으로 실어 보냄) */
	FString Local_ResolveHostAddress() const;

	/** [로컬] 세션 생성 완료 콜백. 성공하면 로비를 엶. */
	void Local_HandleCreateSessionComplete(const UE::Online::TOnlineResult<UE::Online::FCreateSession>& InResult);

	/** [로컬] 세션 검색 완료 콜백. 결과를 표시용 목록으로 옮기고 방송함. */
	void Local_HandleFindSessionsComplete(const UE::Online::TOnlineResult<UE::Online::FFindSessions>& InResult);

	/**
	 * [로컬] 세션 정보를 읽어 검색 결과 항목을 만듦.
	 * @param InSession 값을 읽어올 세션
	 * @return 세션 정보를 담은 검색 결과 항목
	 */
	FCBSessionSearchEntry Local_BuildSearchEntry(const UE::Online::ISession& InSession) const;

	/** [로컬] 세션 참가 완료 콜백. 성공하면 호스트 주소로 접속함. */
	void Local_HandleJoinSessionComplete(const UE::Online::TOnlineResult<UE::Online::FJoinSession>& InResult);

	/** [로컬] 세션 작업 실패를 로그로 남기고 사유를 방송함. */
	void Local_HandleSessionFailure(const FText& InFailureReason, const FString& InLogContext);

	/**
	 * [로컬] 활성 세션에서 빠져나옴. 호스트면 파괴하고, 참가자면 떠나기만 함.
	 * 세션이 없으면 아무것도 하지 않음. 완료를 기다리지 않음
	 */
	void Local_LeaveActiveSession();

	/**
	 * [서버] 광고 중인 세션 설정을 갱신함. 호스트가 아니거나 활성 세션이 없으면 아무것도 하지 않음.
	 * @param InMutations 적용할 세션 설정 변경분
	 */
	void Auth_UpdateAdvertisedSession(UE::Online::FSessionSettingsUpdate&& InMutations);

	/**
	 * [로컬][LAN전용] 온라인 서비스 인스턴스를 파괴해 세션 캐시를 비움.
	 * 검색으로 발견한 세션 캐시를 정리하기 위함. 정리안하면 LAN환경에서 세션을 광고할 때 세션 캐시까지 같이 광고함.
	 */
	void Local_ResetOnlineServices();

	/**
	 * 세션이 LAN 전용인지.
	 * Null 제공자는 LAN 비콘으로만 검색되므로 true 여야 하고, EOS 는 false 여야 함.
	 */
	UPROPERTY(Config)
	bool bUseLANSessions = true;

	/** 로컬 세션 이름. 생성·참가·나가기에서 같은 이름을 씀 (한 번에 하나만 유지). */
	static const FName SessionName;

	/** 세션 스키마 이름. CreateSession 은 이 값이 비어 있으면(NAME_None) invalid_params 로 거부한다. */
	static const FName SessionSchemaName;

	/** 호스트 주소를 실어 보내는 세션 속성 키. 제공자와 무관하게 이 키 하나로 주고받음. */
	static const FName HostAddressSettingKey;

	/** 방 이름을 실어 보내는 세션 속성 키. */
	static const FName DisplayNameSettingKey;

	/** 현재 인원을 실어 보내는 세션 속성 키. 호스트만 갱신하고, 검색 측은 읽기만 함. */
	static const FName CurrentPlayersSettingKey;

	/** 서버 정원을 넘기는 URL 옵션 이름. AGameSession::InitOptions 가 이 이름으로 읽음(엔진 규약). */
	static const FString MaxPlayersUrlOption;

	/** 마지막 검색 결과 (표시용). GetFoundSessions() 로 노출됨 */
	UPROPERTY()
	TArray<FCBSessionSearchEntry> FoundSessions;

	/** FoundSessions 와 인덱스가 대응하는 실제 세션 ID 목록. 참가 시 사용 */
	TArray<UE::Online::FOnlineSessionId> FoundSessionIds;

	/** 세션 생성이 끝나면 열 로비 레벨. 생성 요청 시점에 보관해 뒀다가 콜백에서 사용 */
	TSoftObjectPtr<UWorld> PendingLobbyLevel;

	/** 세션 광고와 서버 정원에 함께 쓸 인원. 생성 요청 시점에 보관해 뒀다가 콜백에서 사용. */
	int32 PendingMaxPlayers = 0;

	/** 세션에 들어가 있는지. 나갈 때 LeaveSession 을 먼저 부를지 판단함 */
	bool bHasActiveSession = false;

	/** 활성 세션의 호스트가 나 자신인지. 나갈 때 세션을 파괴할지(호스트) 떠나기만 할지(참가자) 결정함 */
	bool bIsSessionHost = false;

	/**
	 * 활성 세션에 들어갈 때 쓴 계정 ID.
	 * 종료 시점(UGameInstance::Shutdown)에는 로컬 플레이어가 이미 제거된 뒤라 다시 조회할 수 없으므로,
	 * 세션에 들어간 시점의 값을 보관해 뒀다가 이탈에 씀
	 */
	UE::Online::FAccountId ActiveSessionAccountId;
#pragma endregion

#pragma region Travel
	/** 실제 접속 — 레벨 열기·ClientTravel·실패 복귀. 세션 유무와 무관하게 동작함. */
public:
	/**
	 * [로컬] 로비 레벨을 리슨 서버로 엶.
	 * @param InLobbyLevel 열 로비 레벨.
	 * @param InMaxPlayers 서버 정원. URL 옵션으로 넘겨 AGameSession 이 접속 거부 판정에 씀. 0 이면 엔진 기본값 유지
	 * @return 레벨 열기를 요청했으면 true
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Session")
	bool Local_HostLobby(TSoftObjectPtr<UWorld> InLobbyLevel, int32 InMaxPlayers = 0);

	/**
	 * [로컬] 주소로 서버에 접속함.
	 * @param InAddress 접속 주소. "IP" 또는 "IP:포트" 형식, 비우면 기본 주소(127.0.0.1)를 씀
	 * @return 접속 시도를 요청했으면 true
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Session")
	bool Local_JoinServerByAddress(const FString& InAddress);

	/**
	 * [로컬] 세션에서 나와 메인 메뉴 레벨로 돌아감.
	 * 세션에 들어가 있으면 먼저 세션에서 빠짐 (호스트면 파괴, 참가자면 떠나기).
	 * 클라이언트는 ClientTravel 로 커넥션을 정리하며 나가고, 호스트는 레벨을 직접 엶.
	 * 호스트가 나가면 남은 클라이언트는 접속 끊김으로 각자 메뉴에 돌아감.
	 * 호출 전에 스택에 올린 위젯을 먼저 제거할 것. (IMC 복구가 스택 변화에 붙어 있음)
	 * @return 이동을 요청했으면 true. 이미 메인 메뉴면 false
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Session")
	bool Local_LeaveToMainMenu();

	/**
	 * [로컬] 보관해 둔 실패 사유를 꺼내고 비움. 메인 메뉴 위젯이 뜬 뒤 한 번 호출할 것.
	 * @param OutFailureReason 표시할 사유
	 * @return 표시할 사유가 있었으면 true (한 번 꺼내면 비워짐)
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Session")
	bool Local_ConsumePendingFailureReason(FText& OutFailureReason);

	/**
	 * [로컬] 접속 실패 시 현재 레벨을 유지해야 하는지. UCBOnlineSession이 엔진 대신 물어보는 창구.
	 * 접속·복귀 판단을 이 서브시스템 하나에 모으기 위해 공개함.
	 * @param InNetDriver 실패한 넷 드라이버
	 * @return 현재 레벨을 유지해야 하면 true (= 엔진의 기본 맵 복귀를 막아야 함)
	 */
	bool Local_ShouldKeepCurrentLevel(const UNetDriver* InNetDriver) const;

	/**
	 * 접속·이동 실패 신호.
	 * 접속 시도 실패는 현재 레벨을 유지하므로 이 신호를 받아 그 자리에서 모달을 띄우면 됨.
	 * 접속 후 끊김은 메인 메뉴를 새로 로드해 구독자가 살아남지 못하므로, 그쪽은 Local_ConsumePendingFailureReason 이 주 경로임
	 */
	UPROPERTY(BlueprintAssignable, Category = "ChainBurst|Session")
	FCBOnConnectionFailed OnConnectionFailed;

private:
	/**
	 * [로컬][PIE테스트전용] 방송된 실패가 이 게임 인스턴스의 것인지 판별함.
	 * GEngine 은 프로세스에 하나라 실패 방송이 프로세스 전체에 간다.
	 * PIE 를 한 창만 띄우면 구독자도 하나라 항상 참이지만,
	 * 여러 창을 한 프로세스에서 띄우면(RunUnderOneProcess) 남의 실패까지 받게 되므로 걸러야 함.
	 * @param InFailureWorld 실패와 함께 방송된 월드. 맵 로드 전 단계(PendingNetGame)에서는 NULL 로 올 수 있음
	 * @param InFailureNetDriver 함께 방송된 넷 드라이버. 없으면(이동 실패 등) nullptr
	 * @return 이 인스턴스가 처리해야 할 실패면 true
	 */
	bool Local_IsOwnFailure(const UWorld* InFailureWorld, const UNetDriver* InFailureNetDriver) const;

	/** [로컬] 넷 커넥션이 끊기거나 접속에 실패했을 때 호출됨. */
	void Local_HandleNetworkFailure(UWorld* InWorld, UNetDriver* InNetDriver, ENetworkFailure::Type InFailureType, const FString& InErrorString);

	/** [로컬] 맵 이동에 실패했을 때 호출됨. */
	void Local_HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type InFailureType, const FString& InErrorString);

	/**
	 * [로컬] 실패 사유를 알리고, 들어가 있던 세션에서 빠져나옴. 필요하면 메인 메뉴 레벨로 되돌림.
	 * 이미 메인 메뉴에 있으면 다시 열지 않음. 스스로 나가는 중이면 사유를 알리지 않음.
	 * @param InFailureReason 표시할 사유
	 * @param bKeepCurrentLevel true 면 이동하지 않고 사유를 방송만 함. false 면 사유를 보관하고 메인 메뉴로 되돌림
	 */
	void Local_HandleConnectionFailure(const FText& InFailureReason, bool bKeepCurrentLevel = false);

	/**
	 * [로컬] 이동할 메인 메뉴 맵 이름(ini 의 GameDefaultMap)을 반환함.
	 * 설정이 비었거나 이미 그 맵에 있으면 빈 문자열. (= 이동할 필요 없음)
	 */
	FString Local_ResolveMainMenuTravelMap() const;

	/** 접속 실패 델리게이트 구독 핸들 */
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;

	/**
	 * 레벨 이동을 넘겨 전달할 실패 사유. 도착한 뒤 Local_ConsumePendingFailureReason 이 꺼내 감.
	 * 이 서브시스템이 게임 인스턴스 소속이라 맵을 넘어 살아남는 것을 이용함
	 */
	FText PendingFailureReason;

	/**
	 * 스스로 나가기를 요청했는지.
	 * 커넥션을 닫으며 나가면 자기 자신에게 접속 끊김이 올라올 수 있어, 그 사유를 경고로 표시하지 않기 위한 게이트.
	 * 새 세션을 시작할 때(호스트·참가) 해제함
	 */
	bool bLeaveRequested = false;
#pragma endregion
};
