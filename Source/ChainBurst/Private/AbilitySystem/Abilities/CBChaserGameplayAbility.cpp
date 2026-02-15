// project
#include "AbilitySystem/Abilities/CBChaserGameplayAbility.h"
#include "Characters/CBChaserCharacter.h"
#include "Controllers/CBChaserController.h"

ACBChaserCharacter* UCBChaserGameplayAbility::GetChaserCharacterFromActorInfo()
{
	if (!CachedChaserCharacter.IsValid())
	{
		CachedChaserCharacter = Cast<ACBChaserCharacter>(CurrentActorInfo->AvatarActor);
	}
	return CachedChaserCharacter.IsValid() ? CachedChaserCharacter.Get() : nullptr;
}

UCBChaserCombatComponent* UCBChaserGameplayAbility::GetChaserCombatComponentFromActorInfo()
{
	if (GetChaserCharacterFromActorInfo() == nullptr)
	{
		return nullptr;
	}
	return GetChaserCharacterFromActorInfo()->GetChaserCombatComponent();
}

ACBChaserController* UCBChaserGameplayAbility::GetChaserControllerFromActorInfo()
{
	if (!CachedChaserController.IsValid())
	{
		CachedChaserController = Cast<ACBChaserController>(CurrentActorInfo->PlayerController);
	}
	return CachedChaserController.IsValid() ? CachedChaserController.Get() : nullptr;
}
