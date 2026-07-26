#pragma once

#include "CoreMinimal.h"
#include "Characters/CBAICharacter.h"
#include "CBRogueCharacter.generated.h"

class UCBCombatComponent;
class UCBRogueCombatComponent;
class UCBRogueLoadout;
class UCBAILoadout;

UCLASS()
class CHAINBURST_API ACBRogueCharacter : public ACBAICharacter
{
	GENERATED_BODY()

public:
	ACBRogueCharacter();

protected:
	//~ Begin ACBAICharacter Interface
	virtual TSoftObjectPtr<UCBAILoadout> GetAILoadout() const override;
	//~ End ACBAICharacter Interface

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Combat")
	TObjectPtr<UCBRogueCombatComponent> RogueCombatComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChainBurst|CharacterData")
	TSoftObjectPtr<UCBRogueLoadout> RogueLoadout = nullptr;

public:
	//~ Begin ICBCombatInterface Interface.
	virtual UCBCombatComponent* GetCBCombatComponent() const override;
	//~ End ICBCombatInterface Interface.

	FORCEINLINE UCBRogueCombatComponent* GetRogueCombatComponent() const { return RogueCombatComponent.Get(); }
};
