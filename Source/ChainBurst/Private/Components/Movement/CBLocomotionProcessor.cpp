// project
#include "Components/Movement/CBLocomotionProcessor.h"
#include "Characters/CBBaseCharacter.h"
#include "CBGameplayTags.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"

// engine
#include "GameFramework/CharacterMovementComponent.h"

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
	if (!UCBAbilitySystemLibrary::GetCBCachedASC(GetOwner(),CachedASC))
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
	if (!UCBAbilitySystemLibrary::GetCBCachedASC(GetOwner(),CachedASC))
	{
		return RunBrakingDeceleration;
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
	// CachedCMC 는 지연 캐싱으로 초기화.

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


