#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/CBDelegates.h"
#include "CBLocalReadySubsystem.generated.h"

/**
 * 로컬 플레이어의 준비 완료 신호를 중계하는 월드 서브시스템.
 */
UCLASS()
class CHAINBURST_API UCBLocalReadySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 로컬 플레이어 준비 완료 신호. 방송 시점보다 늦게 구독했다면 IsLocalPlayerReady() 로 확인할 것. */
	UPROPERTY(BlueprintAssignable, Category = "ChainBurst|Ready")
	FCBOnLocalPlayerReady OnLocalPlayerReady;

	/** [로컬] 준비 완료를 알림. 중복 방송은 무시함. */
	void NotifyLocalPlayerReady();

	/**
	 * 준비 완료 여부.
	 * 방송이 구독보다 먼저 일어날 수 있으므로 항상 확인할 것.
	 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Ready")
	FORCEINLINE bool IsLocalPlayerReady() const { return bLocalPlayerReady; }

private:
	/** 준비 완료가 이미 방송됐는지 */
	bool bLocalPlayerReady = false;
};
