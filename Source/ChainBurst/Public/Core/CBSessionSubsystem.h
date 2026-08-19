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
	 * 이미 메인 메뉴에 있으면 다시 열지 않음.
	 */
	void Local_HandleConnectionFailure(const FText& InFailureReason);

	/** 접속 실패 델리게이트 구독 핸들 */
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};
