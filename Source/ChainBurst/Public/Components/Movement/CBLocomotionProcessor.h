#pragma once

#include "CoreMinimal.h"
#include "Components/CBExtensionComponent.h"
#include "CBAbilitySystemLibrary.h"
#include "Engine/EngineTypes.h"
#include "CBLocomotionProcessor.generated.h"

class ACharacter;
class UCharacterMovementComponent;
class UCBAbilitySystemComponent;

UCLASS()
class CHAINBURST_API UCBLocomotionProcessor : public UCBExtensionComponent
{
	GENERATED_BODY()
public:
	UCBLocomotionProcessor();

protected:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	float CalculateMaxAcceleration();
	float CalculateBrakingDeceleration();

#pragma region DerivedMovementTags
	// 파생 이동 태그 로컬 미러링
	// 이 컴포넌트가 파생 이동 태그(Status.Movement.Idle/InAir, Gait.Run)의 단일 소유자.
	// 의도 태그(Gait.Walk/Sprint — GE 소유)는 여기서 읽기만 하고, 부여/제거는 GE가 함.
	// 각 머신이 자기 CMC 데이터를 읽어 비복제 루스 태그를 로컬 부여/제거 — CMC가 복제되므로 서버/오너/프록시가 각자 같은 결론에 도달.

	/** 파생 이동 태그 미러링 초기화 (델리게이트/태그 이벤트 구독 + 초기 상태 반영) */
	void InitializeDerivedMovementTags();

	/** Idle 파생 상태 갱신 (매 틱 — 속도 기반 연속 판정이라 틱 필요, 히스테리시스 적용) */
	void UpdateIdleStateTag();

	/** Gait.Run 파생 갱신 — Walk/Sprint 의도 태그가 둘 다 없을 때의 기본 개이트 (파생-배타) */
	void UpdateRunGaitTag();

	/** 전이 시점에만 루스 태그 부여/제거 (루스 태그는 카운트 방식 — 매 틱 Add 호출 시 카운트 누적되기에 방지) */
	void ApplyLocalStateTag(const FGameplayTag& Tag, bool bShouldApply, bool& bAppliedFlag);

	/** 무브먼트 모드 변경 콜백 (InAir 미러링 — 전환 시점에만 실행) */
	UFUNCTION()
	void OnMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	/** 개이트 의도 태그(Walk/Sprint) 변경 콜백 (Run 파생 갱신) */
	void OnGaitTagChanged(const FGameplayTag Tag, int32 NewCount);
#pragma endregion DerivedMovementTags

	TWeakObjectPtr<UCharacterMovementComponent> CachedCMC;
	
	TWeakObjectPtr<UCBAbilitySystemComponent> CachedASC;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Acceleration")
	float WalkMaxAcceleration = 1000.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Acceleration")
	float RunMaxAcceleration = 1000.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Acceleration")
	float SprintMaxAcceleration = 1000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Deceleration")
	float WalkBrakingDeceleration = 1000.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Deceleration")
	float RunBrakingDeceleration = 1000.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Deceleration")
	float SprintBrakingDeceleration = 1000.0f;

	/** 대시 중(Status.Movement.Dashing) 감속. 루트모션 대시는 속도가 매우 높아 개이트 감속과 별도로 튜닝 — 개이트보다 우선 적용 */
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Deceleration")
	float DashBrakingDeceleration = 2000.0f;

	/** 대시 태그가 사라진 뒤에도 대시 감속을 유지할 시간 (초). 몽타주 종료 후 잔여 고속 구간까지 대시 감속으로 처리 */
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Deceleration")
	float DashBrakingLingerTime = 1.0f;

	/** 대시 태그를 마지막으로 감지한 월드 시각 (linger 판정용) */
	double LastDashTagSeenTime = -DBL_MAX;

	/** Idle 진입 속도 임계 — 무가속 && 수평 속도가 이 값 이하이면 Idle (ABP IsMoving()의 기준 10과 일치) */
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|DerivedMovement")
	float IdleEnterSpeedThreshold = 10.0f;

	/** Idle 이탈 속도 임계 — 진입보다 높게 잡아 경계 플래핑 방지 (히스테리시스) */
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|DerivedMovement")
	float IdleExitSpeedThreshold = 15.0f;

	/** 파생 이동 태그 부여 여부 플래그 (이미 적용되었는지 여부 플래그) */
	bool bIdleTagApplied = false;
	bool bInAirTagApplied = false;
	bool bRunTagApplied = false;

	/** 개이트 의도 태그 이벤트 구독 해제용 핸들 */
	FDelegateHandle WalkTagChangedHandle;
	FDelegateHandle SprintTagChangedHandle;

protected:
	/** 캐릭터 시스템이 완료되었을 때 실행될 초기화 함수 (Tick 활성화) */
	virtual void OnCharacterSystemReady() override;
	
	/**
	 * CMC 를 지연 캐싱해서 가져오는 함수.
	 * @param OutCMC 캐싱된 CMC 포인터를 참조로 전달. 이미 유효한 포인터가 있으면 그대로 반환, 그렇지 않으면 캐싱 시도 후 반환.
	 * @return 성공적으로 가져왔거나 이미 유효하면 true 반환
	 */
	bool GetCachedCMC(TWeakObjectPtr<UCharacterMovementComponent>& OutCMC);

	/**
	 * CBLocomotionProcessor 전용 내부 헬퍼 함수. ASC 를 지연 캐싱해서 가져오는 함수.
	 * @return ASC 를 지연 캐싱해서 가져오는 함수. 이미 유효한 포인터가 있으면 그대로 반환, 그렇지 않으면 캐싱 시도 후 반환.
	 */
	FORCEINLINE UCBAbilitySystemComponent* GetASC() { 
		UCBAbilitySystemLibrary::GetCBCachedASC(GetOwner(), CachedASC); 
		return CachedASC.Get(); 
	}
};
