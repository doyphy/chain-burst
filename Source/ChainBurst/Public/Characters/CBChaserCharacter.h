#pragma once

#include "CoreMinimal.h"
#include "Characters/CBBaseCharacter.h"
#include "GameplayTagContainer.h"
#include "Types/CBDelegates.h"
#include "CBChaserCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UCBInputConfig;
struct FInputActionValue;
class UCBCombatComponent;
class UCBChaserCombatComponent;
class UCBCameraControlComponent;
class UCBCharacterRotationComponent;
class UCBCharacterAnimInstance;
class UCBChaserLoadout;

UCLASS()
class CHAINBURST_API ACBChaserCharacter : public ACBBaseCharacter
{
	GENERATED_BODY()
public:
	ACBChaserCharacter();
	
protected:
	//~ Begin APawn Interface.
		/** [서버] 캐릭터가 컨트롤러에 의해 소유될 때 호출되는 함수. */
	virtual void PossessedBy(AController* NewController) override;
		/** [클라이언트] PlayerState가 변경될 때 호출되는 함수. */
	virtual void OnRep_PlayerState() override;
	//~ End APawn Interface

	/** 컴포넌트 초기화 후에 호출되는 함수. 컴포넌트가 모두 생성되고 초기화된 후에 추가 설정이 필요한 경우 이 함수에서 처리. */
	virtual void PostInitializeComponents() override;
	
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;
	
protected:
#pragma region Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Camera")
	TObjectPtr<UCameraComponent> CameraComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Camera")
	TObjectPtr<USpringArmComponent> SpringArmComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Camera")
	TObjectPtr<UCBCameraControlComponent> CBCameraControlComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Rotation")
	TObjectPtr<UCBCharacterRotationComponent> CBCharacterRotationComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Combat")
	TObjectPtr<UCBChaserCombatComponent> ChaserCombatComponent = nullptr;
#pragma endregion

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChainBurst|CharacterData")
	TSoftObjectPtr<UCBChaserLoadout> ChaserLoadout = nullptr;
	
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
	
	/**
	 * CachedAnimInstance 를 지연 캐싱해서 가져오는 함수.
	 * @param OutAnimInstance 캐싱된 애님 인스턴스 포인터를 참조로 전달. 이미 유효한 포인터가 있으면 그대로 반환, 그렇지 않으면 캐싱 시도 후 반환.
	 * @return 성공적으로 가져왔거나 이미 유효하면 true 반환.
	 */
	bool GetCachedAnimInstance(TObjectPtr<UCBCharacterAnimInstance>& OutAnimInstance);

	/**
	 * 서버와 클라이언트 모두에서 호출되는 초기화 진입 함수
	 * PossessedBy 또는 OnRep_PlayerState에서 호출됨. 
	 */
	void InitializePlayerSystem();

	/** 서버 전용 초기화 함수 */
	void Auth_InitServerData();

	/** 클라이언트 전용 초기화 함수 */
	void Local_InitClientData();

	/** [서버 전용] 캐릭터의 초기 속성 값을 설정하는 함수 */
	void Auth_InitializeAttributes();

	/** 이동 속도 변경 시 실행되는 함수 */
	void OnMovementSpeedChanged(const FOnAttributeChangeData& Data);
	
public:
	//~ Begin ICBCombatInterface Interface.
	virtual UCBCombatComponent* GetCBCombatComponent() const override;
	//~ End ICBCombatInterface Interface.
	
	FORCEINLINE UCBChaserCombatComponent* GetChaserCombatComponent() const { return ChaserCombatComponent.Get(); } 
};
