#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CBCombatInterface.generated.h"

class UCBCombatComponent;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UCBCombatInterface : public UInterface
{
	GENERATED_BODY()
};

class CHAINBURST_API ICBCombatInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual UCBCombatComponent* GetCBCombatComponent() const = 0;
};
