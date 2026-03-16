#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "CBBaseCharacter.generated.h"

class UCBAbilitySystemComponent;
class UCBAttributeSet;
class UCBCharacterLoadout;
class UCBCharacterMovementData;
class UCBCombatComponent;
struct FOnAttributeChangeData;
class UCBLocomotionProcessor;
class UCBCharacterTrajectoryComponent;

UCLASS()
class CHAINBURST_API ACBBaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	ACBBaseCharacter();

	//~ Begin IAbilitySystemInterface Interface.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface Interface.

protected:
	//~ Begin APawn Interface.
		/** [서버] 캐릭터가 컨트롤러에 의해 소유될 때 호출되는 함수. */
	virtual void PossessedBy(AController* NewController) override;
		/** [클라이언트] PlayerState가 변경될 때 호출되는 함수. */
	virtual void OnRep_PlayerState() override;
	//~ End APawn Interface

	/** 캐릭터의 초기 속성 값을 설정하는 함수 */
	void InitializeAttributes();

	/** 이동 속도 변경 시 실행되는 함수 */
	void OnMovementSpeedChanged(const FOnAttributeChangeData& Data);

#pragma region Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|AbilitySystem")
	TObjectPtr<UCBAbilitySystemComponent> CBAbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCBCombatComponent> CBCombatComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|AbilitySystem")
	TObjectPtr<UCBAttributeSet> CBAttributeSet;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Movement")
	TObjectPtr<UCBCharacterTrajectoryComponent> CBTrajectoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Movement")
	TObjectPtr<UCBLocomotionProcessor> CBLocomotionProcessor;
#pragma endregion
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChainBurst|CharacterData")
	TSoftObjectPtr<UCBCharacterLoadout> CharacterLoadout;

	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Movement")
	TObjectPtr<UCBCharacterMovementData> MovementDataAsset;
	
public:
	FORCEINLINE UCBAbilitySystemComponent* GetCBAbilitySystemComponent() const { return CBAbilitySystemComponent; }
	FORCEINLINE UCBAttributeSet* GetCBAttributeSet() const { return CBAttributeSet; }
	FORCEINLINE UCBCombatComponent* GetCBCombatComponent() const { return CBCombatComponent; }
	FORCEINLINE UCBCharacterMovementData* GetMovementDataAsset() const { return MovementDataAsset; }
	FORCEINLINE UCBCharacterTrajectoryComponent* GetCBTrajectoryComponent() const { return CBTrajectoryComponent; }
};
