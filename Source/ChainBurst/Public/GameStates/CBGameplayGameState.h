#pragma once

#include "CoreMinimal.h"
#include "GameStates/CBGameStateBase.h"
#include "Types/CBDelegates.h"
#include "CBGameplayGameState.generated.h"

/**
 * 게임플레이 레벨의 게임 스테이트.
 * 전 플레이어가 공유하는 레벨·경험치를 들고 전 클라이언트에 복제함 (경험치 바 표시용).
 * 값을 정하는 규칙(경험치 누적·레벨 판정)은 게임모드(ACBGameplayGameMode)가 담당하고, 여기는 결과만 받음.
 * 다음 레벨까지 필요한 경험치는 복제하지 않음 - 각 인스턴스가 레벨업 데이터(1인 기준) x 접속 인원(PlayerArray)으로 같은 값을 계산할 수 있음.
 * 서버 판정과 경험치 바 표시가 모두 GetRequiredExperience 하나를 씀.
 * 레벨 시작 시각을 복제해, 경과 시간(GetMatchElapsedTime)을 서버(클리어 기록)·클라(HUD 시계)가 같은 기준으로 계산함.
 */
UCLASS()
class CHAINBURST_API ACBGameplayGameState : public ACBGameStateBase
{
	GENERATED_BODY()

public:
	/** 레벨·경험치가 바뀌었음을 알리는 델리게이트. (경험치 바 위젯에서 구독) */
	UPROPERTY(BlueprintAssignable, Category = "ChainBurst|LevelUp")
	FCBOnExperienceChanged OnExperienceChanged;

	/**
	 * [서버] 게임모드가 계산한 레벨·경험치를 반영하고 방송함. (서버에서만 실행)
	 * 레벨업 직후 월드가 정지돼도 값이 나가도록 즉시 송출을 예약함.
	 * @param InLevel 현재 레벨
	 * @param InExperience 현재 레벨에서 쌓인 경험치
	 */
	void Auth_SetExperience(int32 InLevel, int32 InExperience);

	/** [Getter] 현재 레벨 (1부터). AActor::GetLevel(소속 ULevel)과 이름이 겹쳐 Current 를 붙임 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|LevelUp")
	FORCEINLINE int32 GetCurrentLevel() const { return Level; }

	/** [Getter] 현재 레벨에서 쌓인 경험치 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|LevelUp")
	FORCEINLINE int32 GetExperience() const { return Experience; }

	/**
	 * 지금 레벨에서 다음 레벨까지 필요한 경험치 = 레벨업 데이터의 1인 기준 값 x 접속 인원. (서버 판정·경험치 바 표시 공용)
	 * 인원은 전 클라에 복제되는 PlayerArray 로 세므로 클라도 같은 값을 계산함. 인원이 바뀌면 OnPlayerListChanged 가 방송됨
	 * @param InLevel 현재 레벨 (1부터)
	 * @return 다음 레벨까지 필요한 경험치 (레벨업 데이터가 없으면 0)
	 */
	int32 GetRequiredExperience(int32 InLevel) const;

	/**
	 * 이 레벨이 시작된 뒤 흐른 시간(초). (HUD 시계·클리어 기록 공용)
	 * 서버 월드 시간 기준이라 레벨업 카드를 고르는 동안(월드 정지)은 멈춤. 클라는 엔진이 복제·보정한 서버 시간으로 계산함
	 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Match")
	float GetMatchElapsedTime() const;

protected:
	//~ Begin AActor Interface.
	/** [서버] 레벨 시작 시각을 기록함. */
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor Interface.

	/**
	 * 이 레벨이 시작된 서버 월드 시각(초).
	 * seamless travel 은 이전 레벨의 월드 시간을 그대로 이어받으므로(세션 시작부터 흐름) 레벨 시작 시각을 따로 기록해 빼야 함
	 */
	UPROPERTY(Replicated)
	double MatchStartTime = 0.0;

	/** 레벨·경험치가 바뀌었을 때 호출되는 콜백. */
	UFUNCTION()
	void OnRep_Experience();

	/** 현재 레벨 (1부터) */
	UPROPERTY(ReplicatedUsing = OnRep_Experience)
	int32 Level = 1;

	/** 현재 레벨에서 쌓인 경험치 (레벨업 때 필요량만큼 빠짐) */
	UPROPERTY(ReplicatedUsing = OnRep_Experience)
	int32 Experience = 0;
};
