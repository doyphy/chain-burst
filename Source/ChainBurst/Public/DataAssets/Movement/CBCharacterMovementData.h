#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CBCharacterMovementData.generated.h"

/** 개이트(Walk/Run/Sprint) 하나에 대한 이동 데이터 — 최대 속도와 회전 보간 속도를 함께 묶는다. */
USTRUCT(BlueprintType)
struct FCBGaitMovementData
{
	GENERATED_BODY()

	/** 최대 이동 속도 (cm/s). CMC MaxWalkSpeed로 반영된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float MaxSpeed = 0.0f;

	/** 회전 보간 속도 (RInterpTo 속도). 값이 작을수록 회전이 느리다 — 질주 시 낮게 준다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float RotationInterpSpeed = 5.0f;

	/** 피벗 판정 각도 임계값 (도). 이동 입력 방향이 현재 속도 방향과 이 각도 이상 어긋나면 피벗. 낮을수록 민감 (예: Sprint 90, Walk 160) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float PivotAngleThreshold = 135.0f;

	/** 피벗 시 이동 입력 잠금 시간 (초). 잠금 동안 자연 감속 → Stop 재생 → 해제 후 유지 중인 입력으로 재출발 (예: Walk 0.1, Sprint 0.35) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float PivotInputLockDuration = 0.2f;
};

UCLASS()
class CHAINBURST_API UCBCharacterMovementData : public UDataAsset
{
	GENERATED_BODY()

protected:
	/**
	 * GameplayTag(개이트)에 따른 이동 데이터 매핑 (속도 + 회전 보간 속도)
	 * Key: Status.Movement.Gait.Run    -> { MaxSpeed: 550, RotationInterpSpeed: 5 }
	 * Key: Status.Movement.Gait.Sprint -> { MaxSpeed: 700, RotationInterpSpeed: 2 }
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Gaits", meta = (Categories = "Status.Movement.Gait"))
	TMap<FGameplayTag, FCBGaitMovementData> MovementDataMap;

public:
	/**
	 * 태그에 해당하는 최대 이동 속도를 반환합니다.
	 * @param InTag 조회할 이동 상태(개이트) 태그
	 * @return 태그에 해당하는 이동 속도 반환, 없으면 0.0f를 반환합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	float GetSpeedForTag(FGameplayTag InTag) const;

	/**
	 * 태그에 해당하는 회전 보간 속도를 반환합니다.
	 * @param InTag 조회할 이동 상태(개이트) 태그
	 * @return 태그에 해당하는 회전 보간 속도 반환, 없으면 0.0f를 반환합니다(호출부에서 폴백 처리(예외 처리)).
	 */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	float GetRotationInterpSpeedForTag(FGameplayTag InTag) const;

	/**
	 * 태그에 해당하는 개이트 데이터 구조체를 반환합니다. (C++ 전용)
	 * @param InTag 조회할 이동 상태(개이트) 태그
	 * @return 개이트 데이터 포인터 반환, 없으면 nullptr 반환 (호출부에서 폴백 처리(예외 처리)).
	 */
	const FCBGaitMovementData* FindGaitData(FGameplayTag InTag) const;
};
