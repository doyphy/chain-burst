#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "Net/Core/Connection/NetEnums.h"
#include "Types/CBDelegates.h"
#include "CBSessionSubsystem.generated.h"

/**
 * 멀티플레이 접속 창구 서브시스템.
 * 호스트는 로비 레벨을 리슨 서버로 열고, 참가자는 주소로 접속함.
 * 세션에서 나가는 것도 담당함. (호스트는 레벨을 직접 열고, 클라이언트는 ClientTravel 로 나감)
 * 접속·맵 이동에 실패하면 메인 메뉴로 되돌리고 사유를 방송함. (위젯에서 구독)
 * 맵을 넘어 살아남아야 접속 실패 신호를 받을 수 있으므로 게임 인스턴스 서브시스템으로 둠.
 */
UCLASS()
class CHAINBURST_API UCBSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 서브시스템 생성 시 호출됨. 접속 실패 델리게이트를 구독함. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 서브시스템 해제 시 호출됨. 구독을 해제함. */
	virtual void Deinitialize() override;

	/**
	 * [로컬] 로비 레벨을 리슨 서버로 엶.
	 * @param InLobbyLevel 열 로비 레벨.
	 * @return 레벨 열기를 요청했으면 true
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Session")
	bool Local_HostLobby(TSoftObjectPtr<UWorld> InLobbyLevel);

	/**
	 * [로컬] 주소로 서버에 접속함.
	 * @param InAddress 접속 주소. "IP" 또는 "IP:포트" 형식, 비우면 기본 주소(127.0.0.1)를 씀
	 * @return 접속 시도를 요청했으면 true
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Session")
	bool Local_JoinServerByAddress(const FString& InAddress);

	/**
	 * [로컬] 세션에서 나와 메인 메뉴 레벨로 돌아감.
	 * 클라이언트는 ClientTravel 로 커넥션을 정리하며 나가고, 호스트는 레벨을 직접 엶.
	 * 호스트가 나가면 남은 클라이언트는 접속 끊김으로 각자 메뉴에 돌아감.
	 * 호출 전에 스택에 올린 위젯을 먼저 제거할 것. (IMC 복구가 스택 변화에 붙어 있음)
	 * @return 이동을 요청했으면 true. 이미 메인 메뉴면 false
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Session")
	bool Local_LeaveToMainMenu();

	/** 접속·이동 실패 신호. 메인 메뉴 위젯이 구독해 모달 등으로 표시함. */
	UPROPERTY(BlueprintAssignable, Category = "ChainBurst|Session")
	FCBOnConnectionFailed OnConnectionFailed;

private:
	/** [로컬] 넷 커넥션이 끊기거나 접속에 실패했을 때 호출됨. */
	void Local_HandleNetworkFailure(UWorld* InWorld, UNetDriver* InNetDriver, ENetworkFailure::Type InFailureType, const FString& InErrorString);

	/** [로컬] 맵 이동에 실패했을 때 호출됨. */
	void Local_HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type InFailureType, const FString& InErrorString);

	/**
	 * [로컬] 실패 사유를 방송하고 메인 메뉴 레벨로 되돌림.
	 * 이미 메인 메뉴에 있으면 다시 열지 않음. 스스로 나가는 중이면 사유를 방송하지 않음.
	 */
	void Local_HandleConnectionFailure(const FText& InFailureReason);

	/**
	 * [로컬] 이동할 메인 메뉴 맵 이름(ini 의 GameDefaultMap)을 반환함.
	 * 설정이 비었거나 이미 그 맵에 있으면 빈 문자열. (= 이동할 필요 없음)
	 */
	FString Local_ResolveMainMenuTravelMap() const;

	/** 접속 실패 델리게이트 구독 핸들 */
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;

	/**
	 * 스스로 나가기를 요청했는지.
	 * 커넥션을 닫으며 나가면 자기 자신에게 접속 끊김이 올라올 수 있어, 그 사유를 경고로 표시하지 않기 위한 게이트.
	 * 새 세션을 시작할 때(호스트·참가) 해제함
	 */
	bool bLeaveRequested = false;
};
