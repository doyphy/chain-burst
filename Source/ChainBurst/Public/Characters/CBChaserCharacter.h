#pragma once

#include "CoreMinimal.h"
#include "Characters/CBBaseCharacter.h"
#include "GameplayTagContainer.h"
#include "CBChaserCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UCBInputConfig;
struct FInputActionValue;
class UCBChaserCombatComponent;
class UCBCameraControlComponent;
class UCBCharacterRotationComponent;
class UCBCharacterAnimInstance;

UCLASS()
class CHAINBURST_API ACBChaserCharacter : public ACBBaseCharacter
{
	GENERATED_BODY()
public:
	ACBChaserCharacter();
	
protected:
	//~ Begin APawn Interface.
	virtual void PossessedBy(AController* NewController) override;
	//~ End APawn Interface
	
	virtual void PostInitializeComponents() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;
	
protected:
#pragma region Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera")
	TObjectPtr<UCameraComponent> CameraComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera")
	TObjectPtr<USpringArmComponent> SpringArmComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera")
	TObjectPtr<UCBCameraControlComponent> CBCameraControlComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Rotation")
	TObjectPtr<UCBCharacterRotationComponent> CBCharacterRotationComponent;
#pragma endregion
	
#pragma region Inputs
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChainBurst|Input")
	UCBInputConfig* InputConfig;
	
	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Look(const FInputActionValue& InputActionValue);
	void Input_Camera_Zoom(const FInputActionValue& InputActionValue);

	void Input_AbilityInputPressed(FGameplayTag InInputTag);
	void Input_AbilityInputReleased(FGameplayTag InInputTag);
#pragma endregion

	UPROPERTY(Transient)
	TObjectPtr<UCBCharacterAnimInstance> CachedAnimInstance;

	UFUNCTION()
	void OnAnimInstanceInitialized();
	
public:
	UCBChaserCombatComponent* GetChaserCombatComponent() const;
};
