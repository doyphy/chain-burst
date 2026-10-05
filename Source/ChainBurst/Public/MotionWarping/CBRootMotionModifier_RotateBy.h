#pragma once

#include "CoreMinimal.h"
#include "RootMotionModifier.h"
#include "CBRootMotionModifier_RotateBy.generated.h"

/**
 * 구간 동안 정해진 각도만큼 몸을 돌리는 루트모션 모디파이어 (전방 휩쓸기 → 회전 베기 등).
 *
 * 워프 타겟이 필요 없어 워프(URootMotionModifier_Warp)가 아닌 베이스 모디파이어를 상속함.
 * - 회전 : 구간 진행도(몽타주 위치)에 비례해 선형으로 YawAngle 만큼 돎. 180도 이상·한 바퀴 이상도 가능
 * - 이동 : 애니메이션 원래 이동을 그대로 둠 (회전만 바꿈)
 * 매 프레임 각도를 더하지 않고 "활성화 시점 방향 + 진행도만큼의 각도"를 목표로 계산함.
 * - 엔진이 구간 진입 첫 프레임을 처리하지 않아 생기는 누락을 다음 프레임에 따라잡음
 * - 복제 보정·예측 재생으로 회전이 튀어도 같은 목표로 수렴함
 * 모든 머신이 몽타주 위치만 보고 같은 각도를 계산하므로 서버·클라 결과가 일치함.
 *
 * 구간 안에서는 애니메이션 자체의 회전을 덮어씀 (제자리 클립 전제 - 기존 워프와 같음).
 * 같은 몽타주의 다른 회전 워프(조준 워프 등)와 구간을 겹치지 말 것 - 뒤에 처리된 쪽 회전만 남음.
 */
UCLASS(meta = (DisplayName = "Rotate By"))
class CHAINBURST_API UCBRootMotionModifier_RotateBy : public URootMotionModifier
{
	GENERATED_BODY()

public:
	//~ Begin URootMotionModifier Interface
	/** 매 프레임 호출되어 루트모션 회전을 진행도에 맞는 목표 각으로 바꿈 */
	virtual FTransform ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds) override;
	//~ End URootMotionModifier Interface

protected:
	/**
	 * 구간 동안 돌 각도(도). + 는 위에서 볼 때 시계 방향(오른쪽으로 돎), - 는 반대.
	 * 끝나는 방향은 시작 방향 + 이 값 (360 이면 원래 방향, 180 이면 뒤를 보고 끝남).
	 * 몽타주 위치 기준이라 재생 속도(PlayRate)가 바뀌어도 총 각도는 같음.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	float YawAngle = 180.f;
};
