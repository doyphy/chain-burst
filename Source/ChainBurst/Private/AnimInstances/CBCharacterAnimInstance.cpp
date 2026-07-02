// project
#include "AnimInstances/CBCharacterAnimInstance.h"
#include "Characters/CBBaseCharacter.h"
#include "CBGameplayTags.h"
#include "Components/Movement/CBCharacterTrajectoryComponent.h"
#include "DataAssets/Movement/CBCharacterMovementData.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"

// engine
#include "GameFramework/CharacterMovementComponent.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "Animation/AnimMontage.h"
#include "AlphaBlend.h"

void UCBCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (GetCachedCharacter(CachedCharacter))
	{
		if (CachedCharacter.Get()->bIsCharacterSystemReady)
		{
			// 이미 시스템이 준비된 상태라면 즉시 초기화 함수 실행
			this->OnCharacterSystemReady();
		}
		else
		{
			// 캐릭터 시스템 준비 완료 델리게이트에 바인딩
			CachedCharacter.Get()->OnCharacterSystemReadyDelegate.AddUObject(this, &ThisClass::OnCharacterSystemReady);
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
	MoveX = FMath::FInterpTo(MoveX, LocalVelocity.Y, DeltaSeconds, 10.f);
	MoveY = FMath::FInterpTo(MoveY, LocalVelocity.X, DeltaSeconds, 10.f);
}

void UCBCharacterAnimInstance::UpdateBasicMovementData()
{
	if (GetCachedCharacter(CachedCharacter) && GetCachedCMC(CachedCMC))
	{
		CachedVelocity = CachedCharacter.Get()->GetVelocity();
		CachedAcceleration = CachedCMC.Get()->GetCurrentAcceleration();
		CachedActorRotation = CachedCharacter.Get()->GetActorRotation();
	}
	
	if (GetCachedTrajectoryComp(CachedTrajectoryComp))
	{
		UPoseSearchTrajectoryLibrary::GetTrajectoryVelocity(CachedTrajectoryComp.Get()->GetTrajectory(), 0.3, 0.5, CachedFutureVelocity, false);
	}
}

void UCBCharacterAnimInstance::UpdateCombatAndAbilityData()
{
	if (!GetCachedCharacter(CachedCharacter)) return;
	
	UCBAbilitySystemComponent* ASC = CachedCharacter.Get()->GetCBAbilitySystemComponent();
	
	if (ASC)
	{
		if (ASC->HasMatchingGameplayTag(CBGameplayTags::Movement_Sprint))
		{
			CurrentLocomotionGait = ECBLocomotionGait::Sprint;
		}
		else if (ASC->HasMatchingGameplayTag(CBGameplayTags::Movement_Walk))
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

bool UCBCharacterAnimInstance::GetCachedCharacter(TWeakObjectPtr<ACBBaseCharacter>& OutCharacter)
{
	// 캐싱된 Character가 이미 존재하면 그대로 반환
	if (OutCharacter.IsValid())
	{
		return true;
	}

	// 유효하지 않다면 캐싱 시도
	OutCharacter = Cast<ACBBaseCharacter>(TryGetPawnOwner());
	
	return OutCharacter.IsValid();
}

bool UCBCharacterAnimInstance::GetCachedCMC(TWeakObjectPtr<UCharacterMovementComponent>& OutCMC)
{
	// 캐싱된 CMC가 이미 존재하면 그대로 반환
	if (OutCMC.IsValid())
	{
		return true;
	}

	// 유효하지 않다면 캐싱 시도
	if (GetCachedCharacter(CachedCharacter))
	{
		OutCMC = CachedCharacter.Get()->GetCharacterMovement();
	}
	
	return OutCMC.IsValid();
}

bool UCBCharacterAnimInstance::GetCachedTrajectoryComp(TWeakObjectPtr<UCBCharacterTrajectoryComponent>& OutTrajectoryComp)
{
	// 캐싱된 CMC가 이미 존재하면 그대로 반환
	if (OutTrajectoryComp.IsValid())
	{
		return true;
	}

	// 유효하지 않다면 캐싱 시도
	if (GetCachedCharacter(CachedCharacter))
	{
		OutTrajectoryComp = CachedCharacter.Get()->GetCBTrajectoryComponent();
	}
	
	return OutTrajectoryComp.IsValid();
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

void UCBCharacterAnimInstance::PlayMontage(UAnimMontage* InMontage, float PlayRate)
{
	if (!InMontage) return;

	// 블렌드 인 시간을 재생 속도로 스케일(BlendIn ÷ PlayRate).
	// 블렌드 웨이트는 실제 시간(초) 기준이라, 재생 속도가 빠르면 같은 실시간 블렌드가 몽타주 구간을
	// 더 많이 잠식해 초반 포즈가 뭉개진다(공격 속도가 높을수록 스윙 초반 트레이스 누락 등 문제 발생).
	// 재생 속도로 나눠 몽타주 시간 축에 맞추면 배속과 무관하게 항상 같은 비율만 블렌드한다. (PlayRate=1이면 원래 값 유지)
	FAlphaBlendArgs BlendInArgs = InMontage->GetBlendInArgs();
	if (PlayRate > 0.f)
	{
		BlendInArgs.BlendTime /= PlayRate;
	}

	Montage_PlayWithBlendIn(InMontage, BlendInArgs, PlayRate);
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
		CachedCharacter.Get()->OnCharacterSystemReadyDelegate.RemoveAll(this);
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
	if (auto* MoveData = CachedCharacter.Get()->GetMovementDataAsset())
	{
		WalkMaxSpeed = MoveData->GetSpeedForTag(CBGameplayTags::Movement_Walk);
		RunMaxSpeed = MoveData->GetSpeedForTag(CBGameplayTags::Movement_Run);
		SprintMaxSpeed = MoveData->GetSpeedForTag(CBGameplayTags::Movement_Sprint);
	}

	// 델리게이트 설정
	if (UCBAbilitySystemComponent* ASC = CachedCharacter->GetCBAbilitySystemComponent())
	{
		// 전투 모드 태그 변경 시 OnCombatTagChanged 함수 호출
		ASC->RegisterGameplayTagEvent(
			CBGameplayTags::Status_Combat_InCombat,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UCBCharacterAnimInstance::OnCombatTagChanged);
		
	}

	bIsAnimDataInitialized = true;
}

