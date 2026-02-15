#pragma once

#include "CoreMinimal.h"
#include "AnimInstances/CBBaseAnimInstance.h"
#include "CBCharacterAnimInstance.generated.h"

class ACBBaseCharacter;
class UCharacterMovementComponent;

UCLASS()
class CHAINBURST_API UCBCharacterAnimInstance : public UCBBaseAnimInstance
{
	GENERATED_BODY()
	
public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	
protected:
	UPROPERTY()
	ACBBaseCharacter* OwningCharacter;
	
	UPROPERTY()
	UCharacterMovementComponent* OwningMovementComponent;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	float GroundSpeed;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	bool bHasAcceleration;
};
