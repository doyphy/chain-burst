#pragma once

#include "CoreMinimal.h"
#include "GameFramework/OnlineSession.h"
#include "CBOnlineSession.generated.h"

/**
 * 엔진의 접속 끊김 처리를 가로채는 어댑터.
 * 엔진은 접속에 실패하면 기본 맵(GameDefaultMap)을 다시 로드하는데, 그 경로가 이 클래스의 HandleDisconnect() 하나로 모임.
 *
 * 판단은 UCBSessionSubsystem 이 하고 이 클래스는 전달만 함.
 * 게임 인스턴스가 GetOnlineSessionClass() 로 이 클래스를 지정해야 연결됨.
 */
UCLASS()
class CHAINBURST_API UCBOnlineSession : public UOnlineSession
{
	GENERATED_BODY()

public:
	/**
	 * [로컬] 커넥션이 끊기거나 접속에 실패했을 때 엔진이 호출함.
	 * 기본 구현(Super)은 기본 맵으로 되돌리는 travel 을 예약함
	 */
	virtual void HandleDisconnect(UWorld* InWorld, UNetDriver* InNetDriver) override;
};
