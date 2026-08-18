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
 * InputConfig 캐시, 입력 핸들러, 매핑 컨텍스트 등록·바인딩을 담당.
 *
 * 매핑 컨텍스트는 AllowGameplayInput() 이 불린 뒤에만 등록.
 * 캐릭터를 조작하지 않는 레벨(로비)에서는 아예 붙지 않으므로 어떤 조작도 성립하지 않음.
 * 액션 바인딩은 이 게이트와 무관하게 수행(IMC 가 없으면 발동되지 않음).
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CHAINBURST_API UCBInputManagerComponent : public UCBExtensionComponent
{
	GENERATED_BODY()

#pragma region InputSetup
	/**
	 * 입력을 쓸 수 있게 만드는 과정
	 * 데이터 주입, 지연 셋업, 액션 바인딩, 매핑 컨텍스트 등록.
	 * 입력이 동작할 조건(잠금·허용 플래그)
	 */
public:
	/** 로드아웃에서 입력 설정을 주입하는 세터 (주입 후 셋업을 지연 시도) */
	void SetInputConfig(UCBInputConfig* InInputConfig);

	/** 폰의 SetupPlayerInputComponent에서 호출. 입력 컴포넌트를 받아 셋업을 지연 시도 */
	void SetupPlayerInput(UInputComponent* InInputComponent);

	/** 입력 잠금 여부 설정 (캐릭터 시스템 준비 전까지 이동 입력을 막는 시작 게이트) */
	FORCEINLINE void SetInputLocked(bool bInLocked) { bIsInputLocked = bInLocked; }

	/**
	 * 게임플레이 입력(매핑 컨텍스트)을 허용하는 함수.
	 * 캐릭터를 조작하지 않는 레벨에서는 호출하지 않음.
	 */
	void AllowGameplayInput();

protected:
	/**
	 * 입력 셋업 지연 시도 함수.
	 * InputConfig(로드아웃 로드)와 InputComponent(SetupPlayerInputComponent)는 완료 시점이 서로 다르므로,
	 * 양쪽에서 이 함수를 호출해 준비된 것부터 순서에 상관없이 처리.
	 */
	void TrySetupInput();

	/** 액션 바인딩 지연 시도. InputConfig 와 InputComponent 가 모두 준비되면 1회 수행. */
	void TryBindInputActions();

	/** 매핑 컨텍스트 등록 지연 시도. InputConfig 에 더해 게임플레이 입력이 허용되어야 수행. */
	void TryRegisterMappingContexts();

	/** 캐릭터가 사용하는 매핑 컨텍스트를 데이터에 지정된 우선순위대로 서브시스템에 등록 */
	void RegisterMappingContexts();

	/** 네이티브·어빌리티 입력 액션을 입력 컴포넌트에 바인딩 */
	void BindInputActions();

	/** 입력 설정. 로드아웃(UCBChaserLoadout)에서 주입되는 런타임 캐시 */
	UPROPERTY()
	TObjectPtr<UCBInputConfig> InputConfig = nullptr;

	/** 폰의 입력 컴포넌트 (SetupPlayerInputComponent에서 주입, 주입 시점에 캐스팅해 보관) */
	UPROPERTY(Transient)
	TObjectPtr<UCBInputComponent> CachedInputComponent = nullptr;

private:
	/** 입력 잠금 여부 */
	bool bIsInputLocked = true;

	/** 액션 바인딩 완료 플래그 (지연 바인딩 중복 방지) */
	bool bInputBindingsSetup = false;

	/** 게임플레이 입력 허용 여부. 허용 전에는 매핑 컨텍스트를 등록하지 않음 (로비 등 조작하지 않는 레벨) */
	bool bGameplayInputAllowed = false;

	/** 매핑 컨텍스트 등록 완료 플래그 (중복 등록 방지) */
	bool bMappingContextsRegistered = false;
#pragma endregion

#pragma region InputHandlers
	/** 실제 입력이 들어왔을 때의 처리 — 바인딩된 콜백들과 그 공용 헬퍼. */
protected:
	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Look(const FInputActionValue& InputActionValue);
	void Input_Camera_Zoom(const FInputActionValue& InputActionValue);

	void Input_AbilityInputPressed(FGameplayTag InInputTag);
	void Input_AbilityInputReleased(FGameplayTag InInputTag);

	/** 소유 Chaser 캐릭터를 지연 캐싱해서 반환 (핸들러·피벗 공용) */
	ACBChaserCharacter* GetOwningChaser();

	/** 소유 Chaser 캐릭터 (지연 캐싱) */
	UPROPERTY(Transient)
	TObjectPtr<ACBChaserCharacter> CachedChaser = nullptr;
#pragma endregion

#pragma region Pivot
	/** 피벗 감지·이동 입력 잠금 — 급반전 시 이동 입력을 개이트별 시간만큼 잠가 자연 정지(Stop) → 재출발(Start)을 유도 */
protected:
	/**
	 * 피벗 감지 시도 함수. Input_Move에서 이동 적용 전에 호출.
	 * 이동 입력 방향(카메라 기준 의도)과 현재 속도 방향의 각도가 개이트별 임계값 이상이면 이동 입력을 잠금.
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
};
