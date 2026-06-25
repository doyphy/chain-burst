#pragma once

#include "CoreMinimal.h"
#include "Characters/CBBaseCharacter.h"
#include "CBRogueCharacter.generated.h"

class UCBRogueLoadout;

UCLASS()
class CHAINBURST_API ACBRogueCharacter : public ACBBaseCharacter
{
	GENERATED_BODY()

public:
	ACBRogueCharacter();

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	//~ End AActor Interface

	//~ Begin APawn Interface
	virtual void PossessedBy(AController* NewController) override;
	//~ End APawn Interface

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChainBurst|CharacterData")
	TSoftObjectPtr<UCBRogueLoadout> RogueLoadout = nullptr;
};
