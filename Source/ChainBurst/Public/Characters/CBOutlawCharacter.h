#pragma once

#include "CoreMinimal.h"
#include "Characters/CBAICharacter.h"
#include "CBOutlawCharacter.generated.h"

class UCBCombatComponent;
class UCBOutlawCombatComponent;
class UCBOutlawLoadout;
class UCBAILoadout;

UCLASS()
class CHAINBURST_API ACBOutlawCharacter : public ACBAICharacter
{
	GENERATED_BODY()

public:
	ACBOutlawCharacter();

protected:
	//~ Begin ACBAICharacter Interface
	virtual TSoftObjectPtr<UCBAILoadout> GetAILoadout() const override;
	//~ End ACBAICharacter Interface

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Combat")
	TObjectPtr<UCBOutlawCombatComponent> OutlawCombatComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChainBurst|CharacterData")
	TSoftObjectPtr<UCBOutlawLoadout> OutlawLoadout = nullptr;

public:
	//~ Begin ICBCombatInterface Interface.
	virtual UCBCombatComponent* GetCBCombatComponent() const override;
	//~ End ICBCombatInterface Interface.

	FORCEINLINE UCBOutlawCombatComponent* GetOutlawCombatComponent() const { return OutlawCombatComponent.Get(); }
};
