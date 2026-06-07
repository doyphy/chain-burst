#pragma once

#include "CoreMinimal.h"
#include "Characters/CBBaseCharacter.h"
#include "CBOutlawCharacter.generated.h"

class UCBCombatComponent;
class UCBOutlawCombatComponent;
class UCBOutlawLoadout;

UCLASS()
class CHAINBURST_API ACBOutlawCharacter : public ACBBaseCharacter
{
	GENERATED_BODY()

public:
	ACBOutlawCharacter();

protected:
	//~ Begin APawn Interface
	virtual void PossessedBy(AController* NewController) override;
	//~ End APawn Interface

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
