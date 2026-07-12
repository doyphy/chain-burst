#pragma once

#include "CoreMinimal.h"
#include "Components/CBExtensionComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/TimerHandle.h"
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

#pragma region Pivot
	/** 피벗 감지·이동 입력 잠금 — 급반전 시 이동 입력을 개이트별 시간만큼 잠가 자연 정지(Stop) → 재출발(Start)을 유도 */
protected:
	/**
	 * 피벗 감지 시도 함수. Input_Move에서 이동 적용 전에 호출.
	 * 이동 입력 방향(카메라 기준 의도)과 현재 속도 방향의 각도가 개이트별 임계값 이상이면 이동 입력을 잠근다.
	 * 잠금 시간·각도 임계값은 이동 데이터 에셋(FCBGaitMovementData)에서 개이트별로 조회.
	 * @param InDesiredDir 카메라 기준 이동 입력 방향 (월드 공간)
	 * @return 피벗이 감지되어 잠금이 시작됐으면 true (호출부는 이번 이동 입력을 무시)
	 */
	bool TryDetectPivot(const FVector& InDesiredDir);

	/** 피벗 이동 입력 잠금 해제 함수 (타이머 콜백) */
	void UnlockPivotInput();

	/** 이 비율 이상 속도가 나던 중이어야 피벗 판정 (개이트 최대 속도 대비. 저속 방향 전환은 잠금 없이 그냥 방향 전환) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Pivot")
	float PivotMinSpeedRatio = 0.3f;

private:
	/** 피벗으로 인한 이동 입력 잠금 여부 (시스템 잠금 bIsInputLocked와 별개, 이동 입력만 차단 — Look/줌/어빌리티 입력은 유지) */
	bool bPivotInputLocked = false;

	/** 피벗 잠금 해제 타이머 핸들 */
	FTimerHandle PivotUnlockTimerHandle;
#pragma endregion

private:
	/** 입력 잠금 여부 */
	bool bIsInputLocked = true;

	/** 입력 바인딩 완료 플래그 (지연 바인딩 중복 방지) */
	bool bInputBindingsSetup = false;
};
