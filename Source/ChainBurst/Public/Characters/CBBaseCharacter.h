#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "CBBaseCharacter.generated.h"

class UCBAbilitySystemComponent;
class UCBAttributeSet;
class UCBCharacterLoadout;
class UCBCharacterMovementData;
class UCBCombatComponent;
struct FOnAttributeChangeData;
class UCBLocomotionProcessor;
class UCBCharacterTrajectoryComponent;

DECLARE_MULTICAST_DELEGATE(FOnCharacterSystemReady)

UCLASS()
class CHAINBURST_API ACBBaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	ACBBaseCharacter();

	//~ Begin IAbilitySystemInterface Interface.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface Interface.

	virtual void Tick(float DeltaTime) override;

protected:
	/**
	 * 자식에서 캐싱
	 * [플레이어]는 PlayerState에서 [AI]는 Character에서 ASC와 AttributeSet을 가져오는 방식으로 구현
	 */
	UPROPERTY()
	TObjectPtr<UCBAbilitySystemComponent> CBASC;

	UPROPERTY()
	TObjectPtr<UCBAttributeSet> CBAttributeSet;
	
#pragma region Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCBCombatComponent> CBCombatComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Movement")
	TObjectPtr<UCBCharacterTrajectoryComponent> CBTrajectoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Movement")
	TObjectPtr<UCBLocomotionProcessor> CBLocomotionProcessor;
#pragma endregion
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChainBurst|CharacterData")
	TSoftObjectPtr<UCBCharacterLoadout> CharacterLoadout;

	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Movement")
	TObjectPtr<UCBCharacterMovementData> MovementDataAsset;
	
public:
	/** 캐릭터 시스템 준비 완료 델리게이트 */
	FOnCharacterSystemReady OnCharacterSystemReadyDelegate;
	/** 캐릭터 시스템 준비 완료 여부 (중복 방지 플래그)*/
	bool bIsCharacterSystemReady = false;
	
	FORCEINLINE UCBAbilitySystemComponent* GetCBAbilitySystemComponent() const { return CBASC.Get(); }
	FORCEINLINE UCBAttributeSet* GetCBAttributeSet() const { return CBAttributeSet.Get(); }
	FORCEINLINE UCBCombatComponent* GetCBCombatComponent() const { return CBCombatComponent.Get(); }
	FORCEINLINE UCBCharacterMovementData* GetMovementDataAsset() const { return MovementDataAsset.Get(); }
	FORCEINLINE UCBCharacterTrajectoryComponent* GetCBTrajectoryComponent() const { return CBTrajectoryComponent.Get(); }

protected:
	/** 통합 초기화 함수 (캐릭터 시스템이 완료되면 델리게이트를 방송) */
	virtual void HandleCharacterSystemReady();
};
