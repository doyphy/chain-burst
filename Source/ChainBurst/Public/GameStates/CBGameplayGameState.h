#pragma once

#include "CoreMinimal.h"
#include "GameStates/CBGameStateBase.h"
#include "Types/CBDelegates.h"
#include "CBGameplayGameState.generated.h"

/**
 * 게임플레이 레벨의 게임 스테이트.
 * 전 플레이어가 공유하는 레벨·경험치를 들고 전 클라이언트에 복제함 (경험치 바 표시용).
 * 값을 정하는 규칙(경험치 누적·레벨 판정)은 게임모드(ACBGameplayGameMode)가 담당하고, 여기는 결과만 받음.
 * 다음 레벨까지 필요한 경험치는 복제하지 않음 - 각 인스턴스가 레벨업 데이터에서 같은 값을 계산할 수 있음.
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

protected:
	//~ Begin AActor Interface.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor Interface.

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
