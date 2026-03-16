// project
#include "AnimInstances/CBCharacterAnimInstance.h"
#include "Characters/CBBaseCharacter.h"
#include "CBGameplayTags.h"
#include "Components/Movement/CBCharacterTrajectoryComponent.h"
#include "DataAssets/Movement/CBCharacterMovementData.h"

// engine
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"

void UCBCharacterAnimInstance::NativeInitializeAnimation()
{
	CachedCharacter = Cast<ACBBaseCharacter>(TryGetPawnOwner());
	
	if (CachedCharacter)
	{
		CachedMovementComp = CachedCharacter->GetCharacterMovement();
		CachedTrajectoryComp = CachedCharacter->GetCBTrajectoryComponent();
		
		if (auto* MoveData = CachedCharacter->GetMovementDataAsset())
		{
			WalkMaxSpeed = MoveData->GetSpeedForTag(CBGameplayTags::Shared_Movement_Walk);
			RunMaxSpeed = MoveData->GetSpeedForTag(CBGameplayTags::Shared_Movement_Run);
			SprintMaxSpeed = MoveData->GetSpeedForTag(CBGameplayTags::Shared_Movement_Sprint);
		}
	}
}

void UCBCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	if (CachedCharacter && CachedMovementComp)
	{
		CachedVelocity = CachedCharacter->GetVelocity();
		CachedAcceleration = CachedMovementComp->GetCurrentAcceleration();
		CachedActorRotation = CachedCharacter->GetActorRotation();
		
		UAbilitySystemComponent* ASC = CachedCharacter->GetAbilitySystemComponent();
		if (ASC)
		{
			// 태그를 검사해서 Enum 값으로 매핑
			if (ASC->HasMatchingGameplayTag(CBGameplayTags::Shared_Movement_Sprint))
			{
				CurrentLocomotionGait = ECBLocomotionGait::Sprint;
			}
			else if (ASC->HasMatchingGameplayTag(CBGameplayTags::Shared_Movement_Walk))
			{
				CurrentLocomotionGait = ECBLocomotionGait::Walk;
			}
			else
			{
				// 기본 상태
				CurrentLocomotionGait = ECBLocomotionGait::Run;
			}
		}
	}
	
	if (CachedTrajectoryComp)
	{
		UPoseSearchTrajectoryLibrary::GetTrajectoryVelocity(CachedTrajectoryComp->GetTrajectory(), 0.3, 0.5, CachedFutureVelocity, false);
	}

	// 피벗 감지 시 타이머 리셋 (입력 잠금 시간)
	if (bIsPivoting)
	{
		PivotLockTimer = PivotLockDuration;
	}
	else if (PivotLockTimer > 0.f)
	{
		PivotLockTimer -= DeltaSeconds;
	}
}

void UCBCharacterAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
	
	if(!CachedCharacter || !CachedMovementComp)
	{
		return;
	}

	// 데이터 업데이트
	CurrentVelocity = CachedVelocity;
	CurrentVelocitySize = CurrentVelocity.Size2D();
	
	CurrentAccelerationSize = CachedAcceleration.Size2D();

	FutureVelocity = CachedFutureVelocity;
	FutureVelocitySize = CachedFutureVelocity.Size2D();
	
	// 상태 업데이트
	bHasAcceleration = CurrentAccelerationSize > KINDA_SMALL_NUMBER;
	bIsPivoting = IsPivoting();
	
	// 인터럽트 모드 업데이트
	CurrentInterruptMode = CalculatePoseSearchInterruptMode();
	PreviousLocomotionGait = CurrentLocomotionGait;

	// 로코모션 상태 업데이트
	CurrentLocomotionState = IsMoving()
		? ECBLocomotionState::Moving
		: ECBLocomotionState::Idle;


	
	// 로컬 속도 계산 (캐릭터의 회전에 따라 월드 속도를 로컬 속도로 변환)
	// 로컬 X = 항상 캐릭터 앞뒤
	// 로컬 Y = 항상 캐릭터 좌우
	FVector LocalVelocity = CachedActorRotation.UnrotateVector(CurrentVelocity);
	if (!LocalVelocity.IsNearlyZero())
	{
		// 정규화 (크기를 1로 만듦)
		// 앞(1,0), 오른쪽 45도(0.707, 0.707)
		LocalVelocity.Normalize();
	}

	// 로컬 속도를 보간하여 InputX, InputY에 적용
	// Input [X] -> LocalVelocity [Y]
	// Input [Y] -> LocalVelocity [X]
	// 블렌드 스페이스의 X축이 캐릭터 좌우, Y축이 캐릭터 앞뒤에 매핑되어 있기 때문
	InputX = FMath::FInterpTo(InputX, LocalVelocity.Y, DeltaSeconds, 10.f);
	InputY = FMath::FInterpTo(InputY, LocalVelocity.X, DeltaSeconds, 10.f);
}

bool UCBCharacterAnimInstance::IsStarting() const
{
	if (!bHasAcceleration || IsPivoting()) return false;
	
	return CurrentVelocitySize + 100.f <= FutureVelocitySize;
}

bool UCBCharacterAnimInstance::IsPivoting() const
{
	if (!bHasAcceleration) return false;
	if (CurrentVelocitySize < 100.0f) return false;
	
	FVector CurrentVelocityDir = CurrentVelocity.GetSafeNormal2D();
	FVector FutureVelocityDir = FutureVelocity.GetSafeNormal2D();

	if (FutureVelocityDir.IsNearlyZero()) return false;

	float CurrentYaw = CurrentVelocityDir.Rotation().Yaw;
	float FutureYaw = FutureVelocityDir.Rotation().Yaw;

	float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentYaw, FutureYaw));

	return YawDelta >= 100.0f;
}

bool UCBCharacterAnimInstance::IsMoving() const
{
	return bHasAcceleration || CurrentVelocitySize > 10.f;
}

bool UCBCharacterAnimInstance::IsStopping() const
{
	if (IsPivoting()) return false;
	
	return !bHasAcceleration && CurrentVelocitySize > 10.f;
}

bool UCBCharacterAnimInstance::IsInputLocked() const
{
	return bIsPivoting || PivotLockTimer > 0.f;
}

EPoseSearchInterruptMode UCBCharacterAnimInstance::CalculatePoseSearchInterruptMode()
{
	// 이전 프레임의 상태와 현재 프레임의 상태 비교
	bool bStateChanged = (CurrentLocomotionGait != PreviousLocomotionGait);
	
	if (bStateChanged)
	{
		// 이동 상태가 변경된 경우에는 Interrupt 허용 (빠른 반응을 위해)
		return EPoseSearchInterruptMode::InterruptOnDatabaseChange;
	}
	
	if (bHasAcceleration)
	{
		// 가속 중일 때는 Interrupt 하지 않음 (더 부드러운 전환을 위해)
		return EPoseSearchInterruptMode::DoNotInterrupt;
	}
	
	// 가속이 없는 경우 (정지 또는 감속 중)에는 Interrupt 허용
	// 그리고 계속 포즈가 유효하지 않도록 설정 (정지 애니메이션으로 빠르게 전환하기 위해)
	// 달리던 흐름을 이어가면 발이 미끄러지므로, 이전 흐름을 끊고 새로운 흐름으로 전환하도록 설정
	return EPoseSearchInterruptMode::InterruptOnDatabaseChangeAndInvalidateContinuingPose;
}

