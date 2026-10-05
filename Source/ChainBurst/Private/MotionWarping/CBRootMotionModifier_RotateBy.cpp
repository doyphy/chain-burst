// project
#include "MotionWarping/CBRootMotionModifier_RotateBy.h"

// engine
#include "MotionWarpingAdapter.h"

// 매 프레임 호출되어 루트모션 회전을 진행도에 맞는 목표 각으로 바꿈
FTransform UCBRootMotionModifier_RotateBy::ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds)
{
	const UMotionWarpingBaseAdapter* OwnerAdapter = GetOwnerAdapter();
	if (!OwnerAdapter) return InRootMotion;

	// 구간 진행도 (몽타주 위치 기준이라 프레임 레이트·재생 속도와 무관). 길이가 0 인 구간은 즉시 끝까지 돎.
	const float Duration = EndTime - StartTime;
	const float Alpha = Duration > UE_KINDA_SMALL_NUMBER
		? FMath::Clamp((CurrentPosition - StartTime) / Duration, 0.f, 1.f)
		: 1.f;

	// 목표 = 활성화 시점 방향(엔진이 StartTransform 에 보관) + 진행도만큼의 각도.
	// 누적이 아니라 목표로 계산해 첫 프레임 누락·복제 보정 후에도 정확히 YawAngle 에서 끝남.
	const float TargetYaw = StartTransform.Rotator().Yaw + YawAngle * Alpha;
	const float CurrentYaw = OwnerAdapter->GetCurrentRotation().Rotator().Yaw;
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw);

	// 이동은 애니메이션 그대로, 회전만 수평으로 교체 (ConstantSpeedWarp 와 같은 방식)
	FTransform FinalRootMotion = InRootMotion;
	FinalRootMotion.SetRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(DeltaYaw)));
	return FinalRootMotion;
}
