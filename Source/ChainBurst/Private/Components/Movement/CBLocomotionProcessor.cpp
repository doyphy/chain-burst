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
	
	// 처음에는 Tick 비활성화 (OnPlayerSystemReady 함수에서 활성화)
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCBLocomotionProcessor::BeginPlay()
{
	Super::BeginPlay();

	if (ACBBaseCharacter* OwnerCharacter = GetOwningPawn<ACBBaseCharacter>())
	{
		if (OwnerCharacter->bIsCharacterSystemReady)
		{
			// 이미 시스템이 준비된 상태라면 즉시 초기화 함수 실행
			this->OnCharacterSystemReady();
		}
		else
		{
			// 캐릭터 시스템 준비 완료 델리게이트에 바인딩
			OwnerCharacter->OnCharacterSystemReadyDelegate.AddUObject(this, &ThisClass::OnCharacterSystemReady);
		}
	}
}

void UCBLocomotionProcessor::TickComponent(float DeltaTime, enum ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 매 Tick 마다 CMC 의 가속과 감속을 계산하여 적용.
	if (GetCachedCMC(CachedCMC))
	{
		CachedCMC->MaxAcceleration = CalculateMaxAcceleration();
		CachedCMC->BrakingDecelerationWalking = CalculateBrakingDeceleration();
	}
}

float UCBLocomotionProcessor::CalculateMaxAcceleration()
{
	// ASC 유효하지 않다면 기본 속도 (Run) 반환
	if (!UCBAbilitySystemLibrary::GetCBCachedASC(GetOwner(),CachedASC))
	{
		return RunMaxAcceleration;
	}
	
	if (CachedASC->HasMatchingGameplayTag(CBGameplayTags::Shared_Movement_Sprint))
	{
		return SprintMaxAcceleration;
	}
	else if (CachedASC->HasMatchingGameplayTag(CBGameplayTags::Shared_Movement_Walk))
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
	
	if (CachedASC->HasMatchingGameplayTag(CBGameplayTags::Shared_Movement_Sprint))
	{
		return SprintBrakingDeceleration;
	}
	else if (CachedASC->HasMatchingGameplayTag(CBGameplayTags::Shared_Movement_Walk))
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
	// 델리게이트 구독 해제 (중복 실행 방지)
	if (ACBBaseCharacter* OwnerCharacter = GetOwningPawn<ACBBaseCharacter>())
	{
		OwnerCharacter->OnCharacterSystemReadyDelegate.RemoveAll(this);
	}

	// CachedCMC 는 지연 캐싱으로 초기화.
	
	// Tick 활성화
	SetComponentTickEnabled(true);
}

bool UCBLocomotionProcessor::GetCachedCMC(TObjectPtr<UCharacterMovementComponent>& OutCMC)
{
	// 캐싱된 CMC가 이미 존재하면 그대로 반환
	if (OutCMC)
	{
		return true;
	}

	// 유효하지 않다면 캐싱 시도
	if (ACBBaseCharacter* OwnerCharacter = GetOwningPawn<ACBBaseCharacter>())
	{
		OutCMC = OwnerCharacter->GetCharacterMovement();

		// 초기 값 설정
		if (OutCMC)
		{
			OutCMC->bUseSeparateBrakingFriction = true;
			OutCMC->MaxAcceleration = 1000.0f;
			OutCMC->BrakingDecelerationWalking = 1000.0f;
		}
	}
	
	return (OutCMC != nullptr);
}


