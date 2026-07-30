#pragma once

#include "CoreMinimal.h"
#include "Characters/CBBaseCharacter.h"
#include "GameplayTagContainer.h"
#include "Types/CBDelegates.h"
#include "CBChaserCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UCBInputConfig;
class UCBInputManagerComponent;
class UCBCombatComponent;
class UCBChaserCombatComponent;
class UCBCameraControlComponent;
class UCBCharacterRotationComponent;
class UCBCharacterAnimInstance;
class UCBChaserLoadout;
class UCBModularMeshComponent;

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
	/** [소유 클라이언트 전용] 폰이 로컬에서 입력을 받게 될 때(빙의/재시작 시) 엔진이 호출하는 함수. */
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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Input")
	TObjectPtr<UCBInputManagerComponent> CBInputManagerComponent = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Mesh")
	TObjectPtr<UCBModularMeshComponent> CBModularMeshComponent = nullptr;
#pragma endregion

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChainBurst|CharacterData")
	TSoftObjectPtr<UCBChaserLoadout> ChaserLoadout = nullptr;

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

	/** [동기·전 인스턴스] ASC/AttributeSet 캐싱 및 ActorInfo 초기화 (로드아웃 불필요) */
	void InitAbilitySystem();

	/** [서버 전용] 로드된 로드아웃으로 어빌리티·무기·이펙트를 적용 */
	void Auth_InitServerData(UCBChaserLoadout* InLoadout);

	/** [소유 클라 전용] 로드된 로드아웃으로 입력 설정 등을 적용 */
	void Local_InitClientData(UCBChaserLoadout* InLoadout);

	/** 통합 초기화 완료 처리 (모든 초기화 완료 후 입력 잠금 해제) */
	virtual void HandleCharacterSystemReady() override;
	
public:
	//~ Begin ICBCombatInterface Interface.
	virtual UCBCombatComponent* GetCBCombatComponent() const override;
	//~ End ICBCombatInterface Interface.
	
	FORCEINLINE UCBChaserCombatComponent* GetChaserCombatComponent() const { return ChaserCombatComponent.Get(); }
	FORCEINLINE UCBCameraControlComponent* GetCameraControlComponent() const { return CBCameraControlComponent.Get(); }
	FORCEINLINE UCBCharacterRotationComponent* GetCharacterRotationComponent() const { return CBCharacterRotationComponent.Get(); }
	FORCEINLINE UCBModularMeshComponent* GetModularMeshComponent() const { return CBModularMeshComponent.Get(); }

	/** 로드아웃에서 입력 설정을 주입하는 세터 (입력 매니저 컴포넌트로 전달) */
	void SetInputConfig(UCBInputConfig* InInputConfig);
};
