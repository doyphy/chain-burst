#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBGameplayAbility.h"
#include "CBChaserGameplayAbility.generated.h"

class ACBChaserCharacter;
class UCBChaserCombatComponent;
class ACBChaserController;

UCLASS()
class CHAINBURST_API UCBChaserGameplayAbility : public UCBGameplayAbility
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Ability")
	ACBChaserCharacter* GetChaserCharacterFromActorInfo();

	UFUNCTION(BlueprintPure, Category = "ChainBurst|Ability")
	UCBChaserCombatComponent* GetChaserCombatComponentFromActorInfo();

	UFUNCTION(BlueprintPure, Category = "ChainBurst|Ability")
	ACBChaserController* GetChaserControllerFromActorInfo();
	
private:
	TWeakObjectPtr<ACBChaserCharacter> CachedChaserCharacter;
	TWeakObjectPtr<ACBChaserController> CachedChaserController;
};