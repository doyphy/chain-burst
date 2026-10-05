// project
#include "MotionWarping/CBRootMotionModifier_ConstantSpeedWarp.h"
#include "CBGameplayTags.h"

// engine
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "MotionWarpingAdapter.h"
#include "TimerManager.h"

namespace
{
	// 요청한 이동량 대비 실제 이동이 이 비율보다 작으면 막힌 것으로 봄.
	// 정면으로 부딪히면 거의 0, 비스듬히 스치는 벽이나 경사로는 이보다 커서 미끄러지며 계속 진행함.
	constexpr float BlockedProgressRatio = 0.3f;
}

// 매 프레임 호출되어 루트모션을 일정 속도 이동으로 바꿈
FTransform UCBRootMotionModifier_ConstantSpeedWarp::ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds)
{
	const UMotionWarpingBaseAdapter* OwnerAdapter = GetOwnerAdapter();
	if (!OwnerAdapter) return InRootMotion;

	// 현재 트랜스폼 값 저장
	FTransform FinalRootMotion = InRootMotion;
	const FVector CurrentLocation = OwnerAdapter->GetVisualRootLocation();
	const FQuat CurrentRotation = OwnerAdapter->GetCurrentRotation();

	// 활성화 후 첫 프레임에 타겟 방향·거리 고정.
	if (!bDirectionLocked)
	{
		const FVector ToTarget = GetTargetLocation() - CurrentLocation;
		MoveDirection = ToTarget.GetSafeNormal2D();
		TravelDistance = ToTarget.Size2D();
		MoveStartLocation = CurrentLocation;
		LastLocation = CurrentLocation;
		LastRequestedDistance = 0.f;
		bMoveStopped = MoveDirection.IsNearlyZero(); // 이미 도착 지점 위
		bDirectionLocked = true;
	}

	// 워프 이동이 활성화 되어있으면
	if (bWarpTranslation)
	{
		FVector WorldDelta = FVector::ZeroVector;

		if (!bMoveStopped && !bRootMotionPaused && !bWarpingPaused)
		{
			// 막힘 판정 - 지난 프레임에 요청한 만큼 실제로 움직였는지 (벽·플레이어에 정면으로 막히면 거의 못 움직임)
			const float MovedDistance = FVector::Dist2D(CurrentLocation, LastLocation);
			if (LastRequestedDistance > UE_KINDA_SMALL_NUMBER && MovedDistance < LastRequestedDistance * BlockedProgressRatio)
			{
				bMoveStopped = true;
			}
			else
			{
				// 도착 판정 - 고정 시점부터 이동한 거리로 남은 거리 계산
				const float RemainingDistance = TravelDistance - FVector::Dist2D(CurrentLocation, MoveStartLocation);
				if (RemainingDistance <= UE_KINDA_SMALL_NUMBER)
				{
					bMoveStopped = true;
				}
				else
				{
					// 속도 × 시간만큼, 도착 지점을 넘지 않게
					const float StepDistance = FMath::Min(Speed * DeltaSeconds, RemainingDistance);
					WorldDelta = MoveDirection * StepDistance;
					LastRequestedDistance = StepDistance;
				}
			}
		}

		// 도착 또는 막힘으로 인해 이동이 멈추면
		if (bMoveStopped)
		{
			LastRequestedDistance = 0.f;

			// 멈춘 순간 1회, 지정한 섹션으로 점프 요청
			if (!bStopSectionRequested)
			{
				bStopSectionRequested = true;
				RequestStopSectionJump();
			}
		}
		LastLocation = CurrentLocation;

		// 월드 이동량 → 루트모션 공간 (엔진 SkewWarp 의 "이동 없는 클립" 경로와 같은 변환)
		const FVector LocalDelta = OwnerAdapter->GetBaseVisualRotationOffset().UnrotateVector(CurrentRotation.UnrotateVector(WorldDelta));
		FinalRootMotion.SetTranslation(LocalDelta);
	}

	// 고정한 이동 방향을 바라봄 (수평 회전만)
	if (bWarpRotation && !bRootMotionPaused && !MoveDirection.IsNearlyZero())
	{
		float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentRotation.Rotator().Yaw, MoveDirection.Rotation().Yaw);
		if (WarpMaxRotationRate > 0.f)
		{
			const float MaxDeltaYaw = WarpMaxRotationRate * DeltaSeconds;
			DeltaYaw = FMath::Clamp(DeltaYaw, -MaxDeltaYaw, MaxDeltaYaw);
		}
		FinalRootMotion.SetRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(DeltaYaw)));
	}

	return FinalRootMotion;
}

// 워프 상태가 바뀔 때 호출됨
void UCBRootMotionModifier_ConstantSpeedWarp::OnStateChanged(ERootMotionModifierState LastState)
{
	Super::OnStateChanged(LastState);

	// 구간이 새로 활성화될 때마다 다음 프레임에 방향을 다시 고정
	if (LastState != ERootMotionModifierState::Active && GetState() == ERootMotionModifierState::Active)
	{
		bDirectionLocked = false;
		bMoveStopped = false;
		bStopSectionRequested = false;
	}
}

// [서버] 멈춘 뒤 넘어갈 섹션으로 점프 요청
void UCBRootMotionModifier_ConstantSpeedWarp::RequestStopSectionJump() const
{
	if (StopSectionName.IsNone()) return;

	// 서버 권위로만 판정. 점프는 어빌리티가 큐로 전 머신에 전달함.
	// (모디파이어는 Simulated Proxy·소유 클라에서도 돌지만 거기서는 요청하지 않음)
	const UMotionWarpingBaseAdapter* OwnerAdapter = GetOwnerAdapter();
	AActor* OwnerActor = OwnerAdapter ? OwnerAdapter->GetActor() : nullptr;
	if (!OwnerActor || !OwnerActor->HasAuthority()) return;

	// 이벤트·큐 파라미터에 이름을 실을 수 없어 섹션 인덱스로 전달 (전 머신이 같은 몽타주 에셋을 재생하므로 인덱스가 같음)
	const UAnimMontage* Montage = Cast<UAnimMontage>(Animation.Get());
	const int32 SectionIndex = Montage ? Montage->GetSectionIndex(StopSectionName) : INDEX_NONE;
	if (SectionIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Constant Speed Warp 의 정지 섹션 '%s' 를 몽타주 '%s' 에서 찾지 못함 - 점프하지 않음"),
			*OwnerActor->GetName(), *StopSectionName.ToString(), *GetNameSafe(Animation.Get()));
		return;
	}

	UWorld* World = OwnerActor->GetWorld();
	if (!World) return;

	// 다음 틱에 보냄. 지금은 CMC 이동 처리 도중이라, 그 자리에서 점프하면 루트모션 적용 중에 몽타주 위치가 바뀜.
	const TWeakObjectPtr<AActor> WeakOwner(OwnerActor);
	const TWeakObjectPtr<const UAnimMontage> WeakMontage(Montage);
	World->GetTimerManager().SetTimerForNextTick([WeakOwner, WeakMontage, SectionIndex]()
	{
		AActor* Owner = WeakOwner.Get();
		const UAnimMontage* JumpMontage = WeakMontage.Get();
		if (!Owner || !JumpMontage) return;

		FGameplayEventData Payload;
		Payload.OptionalObject = JumpMontage; // 점프할 몽타주
		Payload.EventMagnitude = static_cast<float>(SectionIndex); // 섹션 인덱스
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, CBGameplayTags::Event_Action_JumpSection, Payload);
	});
}
