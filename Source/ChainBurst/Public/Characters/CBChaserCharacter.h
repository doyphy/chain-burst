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
	
private:
#pragma region Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* CameraComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* SpringArmComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Combat", meta = (AllowPrivateAccess = "true"))
	UCBCameraControlComponent* CameraControlComponent;
#pragma endregion
	
#pragma region Inputs
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterData", meta = (AllowPrivateAccess = "true"))
	UCBInputConfig* InputConfig;
	
	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Look(const FInputActionValue& InputActionValue);
	void Input_Camera_Zoom(const FInputActionValue& InputActionValue);

	void Input_AbilityInputPressed(FGameplayTag InInputTag);
	void Input_AbilityInputReleased(FGameplayTag InInputTag);
#pragma endregion
	
public:
	UCBChaserCombatComponent* GetChaserCombatComponent() const;
};
