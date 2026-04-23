// project
#include "AnimInstances/CBCharacterAnimInstance.h"
#include "Characters/CBBaseCharacter.h"
#include "CBGameplayTags.h"
#include "Components/Movement/CBCharacterTrajectoryComponent.h"
#include "DataAssets/Movement/CBCharacterMovementData.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"

// engine
#include "GameFramework/CharacterMovementComponent.h"

void UCBCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (GetCachedCharacter(CachedCharacter))
	{
		if (CachedCharacter->bIsCharacterSystemReady)
		{
			// 이미 시스템이 준비된 상태라면 즉시 초기화 함수 실행
			this->OnCharacterSystemReady();
		}
		else
		{
			// 캐릭터 시스템 준비 완료 델리게이트에 바인딩
			CachedCharacter->OnCharacterSystemReadyDelegate.AddUObject(this, &ThisClass::OnCharacterSystemReady);
		}
	}
}

void UCBCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	UpdateBasicMovementData();
	
	UpdateCombatAndAbilityData();
	
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
	
	if(!GetCachedCharacter(CachedCharacter) || !GetCachedCMC(CachedCMC) || !GetCachedTrajectoryComp(CachedTrajectoryComp))
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

void UCBCharacterAnimInstance::UpdateBasicMovementData()
{
	if (GetCachedCharacter(CachedCharacter) && GetCachedCMC(CachedCMC))
	{
		CachedVelocity = CachedCharacter->GetVelocity();
		CachedAcceleration = CachedCMC->GetCurrentAcceleration();
		CachedActorRotation = CachedCharacter->GetActorRotation();
	}
	
	if (GetCachedTrajectoryComp(CachedTrajectoryComp))
	{
		UPoseSearchTrajectoryLibrary::GetTrajectoryVelocity(CachedTrajectoryComp->GetTrajectory(), 0.3, 0.5, CachedFutureVelocity, false);
	}
}

void UCBCharacterAnimInstance::UpdateCombatAndAbilityData()
{
	if (!GetCachedCharacter(CachedCharacter)) return;
	
	UCBAbilitySystemComponent* ASC = CachedCharacter->GetCBAbilitySystemComponent();
	
	if (ASC)
	{
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

bool UCBCharacterAnimInstance::GetCachedCharacter(TObjectPtr<ACBBaseCharacter>& OutCharacter)
{
	// 캐싱된 Character가 이미 존재하면 그대로 반환
	if (OutCharacter)
	{
		return true;
	}

	// 유효하지 않다면 캐싱 시도
	OutCharacter = Cast<ACBBaseCharacter>(TryGetPawnOwner());
	
	return (OutCharacter != nullptr);
}

bool UCBCharacterAnimInstance::GetCachedCMC(TObjectPtr<UCharacterMovementComponent>& OutCMC)
{
	// 캐싱된 CMC가 이미 존재하면 그대로 반환
	if (OutCMC)
	{
		return true;
	}

	// 유효하지 않다면 캐싱 시도
	if (GetCachedCharacter(CachedCharacter))
	{
		OutCMC = CachedCharacter->GetCharacterMovement();
	}
	
	return (OutCMC != nullptr);
}

bool UCBCharacterAnimInstance::GetCachedTrajectoryComp(TObjectPtr<UCBCharacterTrajectoryComponent>& OutTrajectoryComp)
{
	// 캐싱된 CMC가 이미 존재하면 그대로 반환
	if (OutTrajectoryComp)
	{
		return true;
	}

	// 유효하지 않다면 캐싱 시도
	if (GetCachedCharacter(CachedCharacter))
	{
		OutTrajectoryComp = CachedCharacter->GetCBTrajectoryComponent();
	}
	
	return (OutTrajectoryComp != nullptr);
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

void UCBCharacterAnimInstance::OnCombatTagChanged(const FGameplayTag InTag, int32 InCount)
{
	bIsCombatMode = (InCount > 0);
}

void UCBCharacterAnimInstance::OnCharacterSystemReady()
{
	// 델리게이트 구독 해제 (중복 실행 방지)
	if (GetCachedCharacter(CachedCharacter))
	{
		CachedCharacter->OnCharacterSystemReadyDelegate.RemoveAll(this);
	}

	// 애니메이션 데이터 초기화 (ASC 준비 완료)
	InitAnimData();
}

// OnCharacterSystemReady 함수에서 호출되는 애니메이션 데이터 초기화 함수. ASC가 준비된 후에 실행되어야 하는 초기화 로직을 포함.
void UCBCharacterAnimInstance::InitAnimData()
{
	if (bIsAnimDataInitialized)
	{
		return;
	}
	
	// 초기 값 설정
	if (auto* MoveData = CachedCharacter->GetMovementDataAsset())
	{
		WalkMaxSpeed = MoveData->GetSpeedForTag(CBGameplayTags::Shared_Movement_Walk);
		RunMaxSpeed = MoveData->GetSpeedForTag(CBGameplayTags::Shared_Movement_Run);
		SprintMaxSpeed = MoveData->GetSpeedForTag(CBGameplayTags::Shared_Movement_Sprint);
	}

	// 델리게이트 설정
	if (UCBAbilitySystemComponent* ASC = CachedCharacter->GetCBAbilitySystemComponent())
	{
		// 전투 모드 태그 변경 시 OnCombatTagChanged 함수 호출
		ASC->RegisterGameplayTagEvent(
			CBGameplayTags::Shared_Status_Combat_InCombat,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UCBCharacterAnimInstance::OnCombatTagChanged);
		
	}

	bIsAnimDataInitialized = true;
}

