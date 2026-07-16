#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBGameplayAbility.h"
#include "Engine/TimerHandle.h"
#include "CBGAChangeSpeed.generated.h"

/**
 * 전력 질주, 걷기 등 캐릭터의 기본 속도를 일시적으로 변경하는 어빌리티의 베이스 클래스.
 * 기본 종료는 입력 릴리즈(InputReleased)이며, bEndWhenNoAcceleration을 켜면 가속도(이동 입력)가
 * 유예 시간 이상 0으로 유지될 때도 자동 종료된다 (예: GA_Sprint — 정지·피벗 잠금 시 Sprint 해제).
 */
UCLASS()
class CHAINBURST_API UCBGAChangeSpeed : public UCBGameplayAbility
{
	GENERATED_BODY()
public:
	UCBGAChangeSpeed();

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	//~ End UGameplayAbility Interface

protected:
	/** 블루프린트 자식(GA_Sprint, GA_Walk)에서 설정할 속도 구분 태그 (개이트 태그) */
	UPROPERTY(EditDefaultsOnly, Category = "CB|Movement", meta = (Categories = "Status.Movement.Gait"))
	FGameplayTag SpeedDataTag;

	/** 공용으로 사용할 속도 변경용 GE 클래스 (GE_MovementModifier) */
	UPROPERTY(EditDefaultsOnly, Category = "CB|Movement")
	TSubclassOf<UGameplayEffect> MovementModifierGEClass;

	/** 적용된 GE를 나중에 제거하기 위해 저장하는 핸들 */
	FActiveGameplayEffectHandle ActiveGEHandle;

#pragma region AutoEndOnNoAcceleration
	/** 가속도(이동 입력) 소실 시 자동 종료 — 정지/피벗 잠금 등으로 이동이 멈추면 속도 태그도 함께 해제 */
protected:
	/** true면 가속도가 NoAccelerationGraceTime 이상 0으로 유지될 때 어빌리티를 자동 종료 (BP 자식에서 설정 — GA_Sprint용) */
	UPROPERTY(EditDefaultsOnly, Category = "CB|Movement")
	bool bEndWhenNoAcceleration = false;

	/** 가속도 0이 이 시간(초) 이상 지속돼야 자동 종료 (한 프레임 입력 공백·방향 전환 노이즈 방지) */
	UPROPERTY(EditDefaultsOnly, Category = "CB|Movement", meta = (EditCondition = "bEndWhenNoAcceleration"))
	float NoAccelerationGraceTime = 0.2f;

private:
	/** 가속도 감시 타이머 콜백 — 유예 시간 이상 무가속이면 EndAbility */
	void CheckAcceleration();

	/** 가속도 감시 타이머 핸들 */
	FTimerHandle AccelCheckTimerHandle;

	/** 가속도가 0이 되기 시작한 월드 시각 (음수 = 현재 가속 중) */
	double ZeroAccelStartTime = -1.0;
#pragma endregion
};
