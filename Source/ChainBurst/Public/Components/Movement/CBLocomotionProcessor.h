#pragma once

#include "CoreMinimal.h"
#include "Components/CBExtensionComponent.h"
#include "CBLocomotionProcessor.generated.h"

class UCharacterMovementComponent;
class UAbilitySystemComponent;

UCLASS()
class CHAINBURST_API UCBLocomotionProcessor : public UCBExtensionComponent
{
	GENERATED_BODY()
public:
	UCBLocomotionProcessor();
	
protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float CalculateMaxAcceleration() const;
	float CalculateBrakingDeceleration() const;
	
	UCharacterMovementComponent* CachedCMC;
	UAbilitySystemComponent* CachedASC;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Acceleration")
	float WalkMaxAcceleration = 1000.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Acceleration")
	float RunMaxAcceleration = 1000.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Acceleration")
	float SprintMaxAcceleration = 1000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Deceleration")
	float WalkBrakingDeceleration = 1000.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Deceleration")
	float RunBrakingDeceleration = 1000.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Deceleration")
	float SprintBrakingDeceleration = 1000.0f;
};
