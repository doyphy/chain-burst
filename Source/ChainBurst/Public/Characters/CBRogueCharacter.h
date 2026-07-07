#pragma once

#include "CoreMinimal.h"
#include "Characters/CBAICharacter.h"
#include "CBRogueCharacter.generated.h"

class UCBRogueLoadout;

UCLASS()
class CHAINBURST_API ACBRogueCharacter : public ACBAICharacter
{
	GENERATED_BODY()

protected:
	//~ Begin ACBAICharacter Interface
	virtual TSoftObjectPtr<UCBCharacterLoadout> GetAILoadout() const override;
	//~ End ACBAICharacter Interface

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChainBurst|CharacterData")
	TSoftObjectPtr<UCBRogueLoadout> RogueLoadout = nullptr;
};
