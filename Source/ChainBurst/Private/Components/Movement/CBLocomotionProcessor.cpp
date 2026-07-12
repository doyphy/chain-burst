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
}

float UCBLocomotionProcessor::CalculateMaxAcceleration()
{
	// ASC 유효하지 않다면 기본 속도 (Run) 반환
	if (!GetASC())
	{
		return RunMaxAcceleration;
	}
	
	if (CachedASC.Get()->HasMatchingGameplayTag(CBGameplayTags::Movement_Sprint))
	{
		return SprintMaxAcceleration;
	}
	else if (CachedASC.Get()->HasMatchingGameplayTag(CBGameplayTags::Movement_Walk))
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

	if (CachedASC.Get()->HasMatchingGameplayTag(CBGameplayTags::Movement_Sprint))
	{
		return SprintBrakingDeceleration;
	}
	else if (CachedASC.Get()->HasMatchingGameplayTag(CBGameplayTags::Movement_Walk))
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

	// Tick 활성화
	SetComponentTickEnabled(true);
}

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


