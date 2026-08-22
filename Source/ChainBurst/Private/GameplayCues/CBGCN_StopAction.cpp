// project
#include "GameplayCues/CBGCN_StopAction.h"

#include "Characters/CBBaseCharacter.h"

bool UCBGCN_StopAction::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (ACBBaseCharacter* Char = Cast<ACBBaseCharacter>(MyTarget))
	{
		Char->RequestStopMontage(0.1f);
	}

	return Super::OnExecute_Implementation(MyTarget, Parameters);
}
