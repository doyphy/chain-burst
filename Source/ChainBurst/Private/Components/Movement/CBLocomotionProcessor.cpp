// project
#include "Components/Movement/CBLocomotionProcessor.h"
#include "Characters/CBBaseCharacter.h"
#include "CBGameplayTags.h"

// engine
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"


UCBLocomotionProcessor::UCBLocomotionProcessor()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UCBLocomotionProcessor::BeginPlay()
{
	Super::BeginPlay();

	if (ACBBaseCharacter* OwnerCharacter = Cast<ACBBaseCharacter>(GetOwner()))
	{
		CachedCMC = OwnerCharacter->GetCharacterMovement();
		CachedASC = OwnerCharacter->GetAbilitySystemComponent();

		if (CachedCMC)
		{
			CachedCMC->bUseSeparateBrakingFriction = true;
			CachedCMC->MaxAcceleration = 1000.0f;
			CachedCMC->BrakingDecelerationWalking = 1000.0f;
		}
	}
}

void UCBLocomotionProcessor::TickComponent(float DeltaTime, enum ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CachedCMC)
	{
		CachedCMC->MaxAcceleration = CalculateMaxAcceleration();
		CachedCMC->BrakingDecelerationWalking = CalculateBrakingDeceleration();
	}
}

float UCBLocomotionProcessor::CalculateMaxAcceleration() const
{
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

float UCBLocomotionProcessor::CalculateBrakingDeceleration() const
{
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
