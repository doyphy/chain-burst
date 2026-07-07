#pragma once

#include "CoreMinimal.h"
#include "Components/CBExtensionComponent.h"
#include "GameplayTagContainer.h"
#include "CBInputManagerComponent.generated.h"

class UCBInputConfig;
class UCBInputComponent;
class UInputComponent;
class ACBChaserCharacter;
struct FInputActionValue;

/**
 * 입력 처리 컴포넌트 (소유 클라이언트 전용 동작).
 * InputConfig 캐시, 입력 핸들러, 매핑 컨텍스트 등록·바인딩을 담당한다.
 * 폰의 InputComponent(UCBInputComponent)는 바인딩 표면으로만 사용한다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CHAINBURST_API UCBInputManagerComponent : public UCBExtensionComponent
{
	GENERATED_BODY()

public:
	/** 로드아웃에서 입력 설정을 주입하는 세터 (주입 후 바인딩을 지연 시도) */
	void SetInputConfig(UCBInputConfig* InInputConfig);

	/** 폰의 SetupPlayerInputComponent에서 호출. 입력 컴포넌트를 받아 바인딩을 지연 시도 */
	void SetupPlayerInput(UInputComponent* InInputComponent);

	/** 입력 잠금 여부 설정 */
	FORCEINLINE void SetInputLocked(bool bInLocked) { bIsInputLocked = bInLocked; }

protected:
	/** 입력 설정. 로드아웃(UCBChaserLoadout)에서 주입되는 런타임 캐시 */
	UPROPERTY()
	TObjectPtr<UCBInputConfig> InputConfig = nullptr;

	/** 폰의 입력 컴포넌트 (SetupPlayerInputComponent에서 주입, 주입 시점에 캐스팅해 보관) */
	UPROPERTY(Transient)
	TObjectPtr<UCBInputComponent> CachedInputComponent = nullptr;

	/** 소유 Chaser 캐릭터 (지연 캐싱) */
	UPROPERTY(Transient)
	TObjectPtr<ACBChaserCharacter> CachedChaser = nullptr;

	/**
	 * 입력 바인딩 지연 시도 함수.
	 * InputConfig(로드아웃 로드)와 InputComponent(SetupPlayerInputComponent)는 완료 시점이 서로 다르므로,
	 * 양쪽에서 이 함수를 호출해 둘 다 준비되었을 때 한 번만 실제 바인딩을 수행한다.
	 */
	void TrySetupInput();

	/** 실제 입력 셋업 수행 (전제조건이 모두 충족되었다고 가정). 매핑 컨텍스트 등록과 액션 바인딩을 순서대로 호출. */
	void SetupInputBindings();

	/** 캐릭터가 사용하는 매핑 컨텍스트를 데이터에 지정된 우선순위대로 서브시스템에 등록 */
	void RegisterMappingContexts();

	/** 네이티브·어빌리티 입력 액션을 입력 컴포넌트에 바인딩 */
	void BindInputActions();

	/** 소유 Chaser 캐릭터를 지연 캐싱해서 반환 */
	ACBChaserCharacter* GetOwningChaser();

#pragma region Inputs
	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Look(const FInputActionValue& InputActionValue);
	void Input_Camera_Zoom(const FInputActionValue& InputActionValue);

	void Input_AbilityInputPressed(FGameplayTag InInputTag);
	void Input_AbilityInputReleased(FGameplayTag InInputTag);
#pragma endregion

private:
	/** 입력 잠금 여부 */
	bool bIsInputLocked = true;

	/** 입력 바인딩 완료 플래그 (지연 바인딩 중복 방지) */
	bool bInputBindingsSetup = false;
};
