// project
#include "MotionWarping/CBRootMotionModifier_ClampedSkewWarp.h"

// engine
#include "MotionWarpingAdapter.h"

// 매 프레임 호출되어 루트모션을 처리함
FTransform UCBRootMotionModifier_ClampedSkewWarp::ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds)
{
	// 베이스는 진짜 타겟을 그대로 보고 이동·회전을 계산 (회전 워프가 타겟을 계속 바라보도록)
	FTransform FinalRootMotion = Super::ProcessRootMotion(InRootMotion, DeltaSeconds);

	// 최대 거리 제한이 없거나 아직 기준점이 없으면 그대로 둠
	if (MaxWarpDistance <= 0.f || !bHasClampOrigin) return FinalRootMotion;

	// 워프를 소유한 어댑터를 가져와서 현재 위치를 확인
	const UMotionWarpingBaseAdapter* OwnerAdapter = GetOwnerAdapter();
	if (!OwnerAdapter) return FinalRootMotion;

	// 워프 시작 지점에서 지금까지 벌어진 거리 (Z축 무시 여부는 워프 설정을 따름)
	const FVector TraveledOffset = OwnerAdapter->GetVisualRootLocation() - ClampOriginLocation;
	const float TraveledDistance = bIgnoreZAxis ? TraveledOffset.Size2D() : TraveledOffset.Size();
	const float RemainingDistance = MaxWarpDistance - TraveledDistance;

	// 상한에 닿았으면 더 밀지 않음 (회전은 건드리지 않으므로 타겟을 계속 바라봄)
	if (RemainingDistance <= 0.f)
	{
		// 위치만 0으로 만들어 루트모션 이동을 막음
		FinalRootMotion.SetTranslation(FVector::ZeroVector);
		return FinalRootMotion;
	}

	// 루트모션 이동량이 남은 거리보다 크면 잘라냄
	const FVector Translation = FinalRootMotion.GetTranslation();
	if (Translation.Size() > RemainingDistance)
	{
		// 위치만 잘라내고 회전은 그대로 둠 (타겟을 계속 바라보도록)
		FinalRootMotion.SetTranslation(Translation.GetClampedToMaxSize(RemainingDistance));
	}

	// 최종 루트모션 반환
	return FinalRootMotion;
}

// 워프 상태가 바뀔 때 호출됨
// 워프 활성화 시점의 위치를 거리 제한 기준점으로 보관
void UCBRootMotionModifier_ClampedSkewWarp::OnStateChanged(ERootMotionModifierState LastState)
{
	Super::OnStateChanged(LastState);

	// 워프가 활성화되는 순간의 위치를 기준점으로 보관. (워프가 끝나면 다시 초기화)
	if (LastState != ERootMotionModifierState::Active && GetState() == ERootMotionModifierState::Active)
	{
		// 워프 시작 시점의 위치를 기준점으로 보관
		ClampOriginLocation = StartTransform.GetLocation();
		bHasClampOrigin = true;
	}
}
