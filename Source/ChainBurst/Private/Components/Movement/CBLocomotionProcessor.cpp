// project
#include "Components/Movement/CBLocomotionProcessor.h"
#include "Characters/CBBaseCharacter.h"
#include "CBGameplayTags.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"

// engine
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

UCBLocomotionProcessor::UCBLocomotionProcessor()
{
	PrimaryComponentTick.bCanEverTick = true;
	
	// 처음에는 Tick 비활성화 (OnCharacterSystemReady 함수에서 활성화)
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCBLocomotionProcessor::TickComponent(float DeltaTime, enum ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 매 Tick 마다 CMC 의 가속과 감속을 계산하여 적용.
	if (GetCachedCMC(CachedCMC))
	{
		CachedCMC.Get()->MaxAcceleration = CalculateMaxAcceleration();
		CachedCMC.Get()->BrakingDecelerationWalking = CalculateBrakingDeceleration();
	}

	// Idle 상태 업데이트 (InAir/Run은 이벤트 기반이라 틱 불필요)
	UpdateIdleStateTag();
}

void UCBLocomotionProcessor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 미러링 태그·구독 정리 — 플레이어 ASC는 PlayerState 소유라 캐릭터보다 오래 살아 태그가 잔류할 수 있음
	if (UCBAbilitySystemComponent* ASC = CachedASC.Get())
	{
		ApplyLocalStateTag(CBGameplayTags::Status_Movement_Idle, false, bIdleTagApplied);
		ApplyLocalStateTag(CBGameplayTags::Status_Movement_InAir, false, bInAirTagApplied);
		ApplyLocalStateTag(CBGameplayTags::Status_Movement_Gait_Run, false, bRunTagApplied);

		// 태그 이벤트 구독 해제 (InitializeDerivedMovementTags 함수에서 구독함)
		ASC->RegisterGameplayTagEvent(CBGameplayTags::Status_Movement_Gait_Walk, EGameplayTagEventType::NewOrRemoved).Remove(WalkTagChangedHandle);
		ASC->RegisterGameplayTagEvent(CBGameplayTags::Status_Movement_Gait_Sprint, EGameplayTagEventType::NewOrRemoved).Remove(SprintTagChangedHandle);
	}

	if (ACharacter* OwnerCharacter = GetOwningPawn<ACharacter>())
	{
		// 무브먼트 모드 변경 델리게이트 해제
		OwnerCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &UCBLocomotionProcessor::OnMovementModeChanged);
	}

	Super::EndPlay(EndPlayReason);
}

float UCBLocomotionProcessor::CalculateMaxAcceleration()
{
	// ASC 유효하지 않다면 기본 속도 (Run) 반환
	if (!GetASC())
	{
		return RunMaxAcceleration;
	}
	
	if (CachedASC.Get()->HasMatchingGameplayTag(CBGameplayTags::Status_Movement_Gait_Sprint))
	{
		return SprintMaxAcceleration;
	}
	else if (CachedASC.Get()->HasMatchingGameplayTag(CBGameplayTags::Status_Movement_Gait_Walk))
	{
		return WalkMaxAcceleration;
	}
	else
	{
		return RunMaxAcceleration;
	}
}

float UCBLocomotionProcessor::CalculateBrakingDeceleration()
{
	// ASC 유효하지 않다면 기본 속도 (Run) 반환
	if (!GetASC())
	{
		return RunBrakingDeceleration;
	}

	// 대시 중이면 개이트와 무관하게 대시 전용 감속 (루트모션 잔여 속도가 개이트 최대 속도보다 훨씬 높아 별도 튜닝 필요)
	if (CachedASC.Get()->HasMatchingGameplayTag(CBGameplayTags::Status_Movement_Dashing))
	{
		// 태그 감지 시각 기록 (linger 판정 기준)
		LastDashTagSeenTime = GetWorld()->GetTimeSeconds();
		return DashBrakingDeceleration;
	}

	// 대시 태그가 사라진 직후에도 잔여 고속 구간 동안은 대시 감속 유지 (linger)
	if (GetWorld()->GetTimeSeconds() - LastDashTagSeenTime < DashBrakingLingerTime)
	{
		return DashBrakingDeceleration;
	}

	if (CachedASC.Get()->HasMatchingGameplayTag(CBGameplayTags::Status_Movement_Gait_Sprint))
	{
		return SprintBrakingDeceleration;
	}
	else if (CachedASC.Get()->HasMatchingGameplayTag(CBGameplayTags::Status_Movement_Gait_Walk))
	{
		return WalkBrakingDeceleration;
	}
	else
	{
		return RunBrakingDeceleration;
	}
}

void UCBLocomotionProcessor::OnCharacterSystemReady()
{
	// CachedCMC 초기화
	GetCachedCMC(CachedCMC);

	// 파생 이동 태그 미러링 초기화 (ASC 준비 완료 이후여야 하므로 여기서)
	InitializeDerivedMovementTags();

	// Tick 활성화
	SetComponentTickEnabled(true);
}

#pragma region DerivedMovementTags

// 파생 이동 태그 미러링 초기화 (델리게이트/태그 이벤트 구독 + 초기 상태 반영)
void UCBLocomotionProcessor::InitializeDerivedMovementTags()
{
	UCBAbilitySystemComponent* ASC = GetASC();
	if (!ASC) return;

	// 개이트 의도 태그(Walk/Sprint) 변경 이벤트 구독 — Run은 "둘 다 없음"에서 파생되므로 이벤트 시점에만 갱신
	WalkTagChangedHandle = ASC->RegisterGameplayTagEvent(CBGameplayTags::Status_Movement_Gait_Walk, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UCBLocomotionProcessor::OnGaitTagChanged);
	SprintTagChangedHandle = ASC->RegisterGameplayTagEvent(CBGameplayTags::Status_Movement_Gait_Sprint, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UCBLocomotionProcessor::OnGaitTagChanged);

	// 무브먼트 모드 변경 델리게이트 구독 (InAir — 전환 시점에만 실행)
	if (ACharacter* OwnerCharacter = GetOwningPawn<ACharacter>())
	{
		OwnerCharacter->MovementModeChangedDelegate.AddDynamic(this, &UCBLocomotionProcessor::OnMovementModeChanged);
	}

	// 초기 상태 반영
	
	// Run 상태 업데이트
	UpdateRunGaitTag();
	// 태그 부여/제거는 ApplyLocalStateTag 함수에서 전이 시점에만 수행되므로, 초기 상태 반영을 위해 강제로 호출
	// 현재 CMC가 유효하고, 캐릭터가 공중에 있는지 여부를 확인하여 InAir 태그를 적용/제거
	ApplyLocalStateTag(CBGameplayTags::Status_Movement_InAir, CachedCMC.IsValid() && CachedCMC.Get()->IsFalling(), bInAirTagApplied);
	// Idle 상태 업데이트
	UpdateIdleStateTag();
}

// Idle 상태 업데이트 (매 틱 — 속도 기반 연속 판정이라 틱 필요, *히스테리시스 적용)
void UCBLocomotionProcessor::UpdateIdleStateTag()
{
	if (!CachedCMC.IsValid()) return;

	const bool bHasAcceleration = CachedCMC.Get()->GetCurrentAcceleration().SizeSquared2D() > KINDA_SMALL_NUMBER;
	const float CurrentSpeed = CachedCMC.Get()->Velocity.Size2D();

	// Idle은 지상 전용
	const bool bIsGrounded = !CachedCMC.Get()->IsFalling();

	bool bShouldBeIdle = bIdleTagApplied;

	// 히스테리시스: 진입(≤ Enter)과 이탈(> Exit) 임계를 분리해 경계 플래핑 방지
	// Idle 태그 적용 여부 플래그를 통해 현재 Idle 상태인지 판단
	if (bIdleTagApplied) // Idle 상태가 이미 적용되어 있다면
	{
		// 이탈 조건(> Exit)으로 판단해서 넘어가면 false (=Idle 상태 해제)
		bShouldBeIdle = bIsGrounded && !bHasAcceleration && CurrentSpeed <= IdleExitSpeedThreshold;
	}
	else // Idle 상태가 적용되어 있지 않다면
	{
		// 진입 조건(≤ Enter)으로 판단해서 넘어가면 true (=Idle 상태 적용)
		bShouldBeIdle = bIsGrounded && !bHasAcceleration && CurrentSpeed <= IdleEnterSpeedThreshold;
	}

	// Idle 태그 부여/제거 (전이 시점에만 수행)
	ApplyLocalStateTag(CBGameplayTags::Status_Movement_Idle, bShouldBeIdle, bIdleTagApplied);
}

// Run 상태 업데이트 (Walk/Sprint 의도 태그가 둘 다 없을 때의 기본 개이트 — 파생-배타)
void UCBLocomotionProcessor::UpdateRunGaitTag()
{
	UCBAbilitySystemComponent* ASC = GetASC();
	if (!ASC) return;

	// Run = Walk/Sprint 의도 태그가 둘 다 없을 때의 기본 개이트 (파생-배타 — 항상 개이트 태그 정확히 1개 유지)
	// true (=Run 상태 적용), false (=Run 상태 해제)
	const bool bShouldBeRun =
		!ASC->HasMatchingGameplayTag(CBGameplayTags::Status_Movement_Gait_Walk) &&
		!ASC->HasMatchingGameplayTag(CBGameplayTags::Status_Movement_Gait_Sprint);

	// Run 태그 부여/제거 (전이 시점에만 수행)
	ApplyLocalStateTag(CBGameplayTags::Status_Movement_Gait_Run, bShouldBeRun, bRunTagApplied);
}

// 전이 시점에만 루스 태그 부여/제거 (루스 태그는 카운트 방식 — 매 틱 Add 호출 시 카운트 누적되기에 방지)
// 적용할 목표 상태(bShouldApply)와 실제 적용 여부 플래그(bAppliedFlag)를 같이 전달받아, 이미 적용된 상태라면 무시하고, 전이 시점에만 루스 태그를 부여/제거
void UCBLocomotionProcessor::ApplyLocalStateTag(const FGameplayTag& Tag, bool bShouldApply, bool& bAppliedFlag)
{
	// 전이 시점에만 부여/제거 (루스 태그는 카운트 방식 — 반복 Add 호출 시 카운트 누적)
	// 목표 상태가 이미 반영돼 있으면 무시 (적용됐는데 또 적용하려는 것, 없는데 또 제거하려는 것)
	if (bShouldApply == bAppliedFlag) return;

	UCBAbilitySystemComponent* ASC = GetASC();
	if (!ASC) return;

	// 비복제 API만 사용
	if (bShouldApply) // 적용이 필요하면
	{
		// 루스 태그 부여
		ASC->AddLooseGameplayTag(Tag);
	}
	else // 제거가 필요하면
	{
		// 루스 태그 제거
		ASC->RemoveLooseGameplayTag(Tag);
	}
	
	// 적용 여부 플래그 갱신
	bAppliedFlag = bShouldApply;
}

// 무브먼트 모드 변경 델리게이트 구독 콜백 함수 (InAir — 전환 시점에만 실행)
void UCBLocomotionProcessor::OnMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	// InAir 태그 부여/제거 (전이 시점에만 수행)
	// 공중에 있는지 여부를 판단하여 InAir 태그를 적용/제거
	ApplyLocalStateTag(CBGameplayTags::Status_Movement_InAir,
		CachedCMC.IsValid() && CachedCMC.Get()->IsFalling(), bInAirTagApplied);
}

// 개이트 의도 태그(Walk/Sprint) 변경 이벤트 구독 콜백 함수
void UCBLocomotionProcessor::OnGaitTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// Run 상태 업데이트
	// 개이트 의도 태그(Walk/Sprint)가 있는지 판단해 Run 태그 부여/제거
	UpdateRunGaitTag();
}

#pragma endregion DerivedMovementTags

bool UCBLocomotionProcessor::GetCachedCMC(TWeakObjectPtr<UCharacterMovementComponent>& OutCMC)
{
	// 캐싱된 CMC가 이미 존재하면 그대로 반환
	if (OutCMC.IsValid())
	{
		return true;
	}

	// 유효하지 않다면 캐싱 시도
	if (ACBBaseCharacter* OwnerCharacter = GetOwningPawn<ACBBaseCharacter>())
	{
		OutCMC = OwnerCharacter->GetCharacterMovement();

		// 초기 값 설정
		if (OutCMC.IsValid())
		{
			OutCMC.Get()->bUseSeparateBrakingFriction = true;
			OutCMC.Get()->MaxAcceleration = 1000.0f;
			OutCMC.Get()->BrakingDecelerationWalking = 1000.0f;
		}
	}
	
	return OutCMC.IsValid();
}


