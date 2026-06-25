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

	UFUNCTION(BlueprintPure, Category = "ChainBurst|Ability")
	FGameplayEffectSpecHandle MakeDamageEffectSpecHandle(TSubclassOf<UGameplayEffect> EffectClass, float InDamage, FGameplayTag InAttackTag, int32 InComboCount);
	
private:
	TWeakObjectPtr<ACBChaserCharacter> CachedChaserCharacter;
	TWeakObjectPtr<ACBChaserController> CachedChaserController;
};