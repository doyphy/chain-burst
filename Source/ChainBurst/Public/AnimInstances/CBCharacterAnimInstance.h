#pragma once

#include "CoreMinimal.h"
#include "AnimInstances/CBBaseAnimInstance.h"
#include "GameplayTagContainer.h"
#include "CBCharacterAnimInstance.generated.h"

class ACBBaseCharacter;
class UCharacterMovementComponent;
class UCBCharacterTrajectoryComponent;
class UCBActionMontageData;

UENUM(BlueprintType)
enum class ECBLocomotionState : uint8
{
	Idle     UMETA(DisplayName = "Idle"),
	Moving    UMETA(DisplayName = "Moving"),
};

UENUM(BlueprintType)
enum class ECBLocomotionGait : uint8
{
	Walk    UMETA(DisplayName = "Walk"),
	Run     UMETA(DisplayName = "Run"),
	Sprint  UMETA(DisplayName = "Sprint")
};

UCLASS()
class CHAINBURST_API UCBCharacterAnimInstance : public UCBBaseAnimInstance
{
	GENERATED_BODY()
	
protected:
	//~ Begin UAnimInstance Interface
	/** 초기화 함수 */
	virtual void NativeInitializeAnimation() override;
	/** 게임 스레드에서 실행되는 업데이트 함수 (UObject 접근 가능) */
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	/** 워커 스레드에서 실행되는 업데이트 함수 (UObject 접근 불가 (외부 접근 불가, 내부 데이터로만), 매우 빠름)*/
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	//~ End UAnimInstance Interface

	/** 기본적인 데이터 처리 함수 (어빌리티, 컴뱃 컴포넌트 불필요) */
	void UpdateBasicMovementData();

	/** 커스텀 데이터 처리 함수 (어빌리티, 컴뱃 컴포넌트 필요) */
	void UpdateCombatAndAbilityData();
	
	UPROPERTY(BlueprintReadOnly, Transient, Category = "References")
	TWeakObjectPtr<ACBBaseCharacter> CachedCharacter = nullptr;
	
	UPROPERTY(BlueprintReadOnly, Transient, Category = "References")
	TWeakObjectPtr<UCharacterMovementComponent> CachedCMC = nullptr;
	
	UPROPERTY(BlueprintReadOnly, Transient, Category = "References")
	TWeakObjectPtr<UCBCharacterTrajectoryComponent> CachedTrajectoryComp = nullptr;
	
	/**
	 * CachedCharacter 를 지연 캐싱해서 가져오는 함수.
	 * @param OutCharacter 캐싱된 캐릭터 포인터를 참조로 전달. 이미 유효한 포인터가 있으면 그대로 반환, 그렇지 않으면 캐싱 시도 후 반환.
	 * @return 성공적으로 가져왔거나 이미 유효하면 true 반환.
	 */
	bool GetCachedCharacter(TWeakObjectPtr<ACBBaseCharacter>& OutCharacter);

	/**
	 * CMC 를 지연 캐싱해서 가져오는 함수.
	 * @param OutCMC 캐싱된 CharacterMovementComponent 포인터를 참조로 전달. 이미 유효한 포인터가 있으면 그대로 반환, 그렇지 않으면 캐싱 시도 후 반환.
	 * @return 성공적으로 가져왔거나 이미 유효하면 true 반환.
	 */
	bool GetCachedCMC(TWeakObjectPtr<UCharacterMovementComponent>& OutCMC);

	/**
	 * CachedTrajectoryComp 를 지연 캐싱해서 가져오는 함수.
	 * @param OutTrajectoryComp 캐싱된 UCBCharacterTrajectoryComponent 포인터를 참조로 전달. 이미 유효한 포인터가 있으면 그대로 반환, 그렇지 않으면 캐싱 시도 후 반환.
	 * @return 성공적으로 가져왔거나 이미 유효하면 true 반환.
	 */
	bool GetCachedTrajectoryComp(TWeakObjectPtr<UCBCharacterTrajectoryComponent>& OutTrajectoryComp);
	
public:
	/** 입력 잠금 (피벗 시 일정 시간동안 입력 잠금하기 위함) */
	bool IsInputLocked() const;

	/**
	 * 몽타주 재생하는 함수.
	 * @param InMontage 재생할 몽타주. 액션 컴포넌트에서 태그로 몽타주 검색 후 전달.
	 * @param PlayRate 재생 속도 배율 (1.0 = 기본). 공격 속도 어트리뷰트를 반영해 UCBActionComponent에서 계산 후 전달.
	 */
	void PlayMontage(UAnimMontage* InMontage, float PlayRate = 1.f);
	
	// ==========================================
	// 워커 스레드에서 계산할 함수 (외부에서 호출 가능)
	// ==========================================
	UFUNCTION(BlueprintPure, Category = "AnimData|LocomotionData", meta = (BlueprintThreadSafe))
	bool IsStarting() const;

	UFUNCTION(BlueprintPure, Category = "AnimData|LocomotionData", meta = (BlueprintThreadSafe))
	bool IsPivoting() const;

	UFUNCTION(BlueprintPure, Category = "AnimData|LocomotionData", meta = (BlueprintThreadSafe))
	bool IsMoving() const;

	UFUNCTION(BlueprintPure, Category = "AnimData|LocomotionData", meta = (BlueprintThreadSafe))
	bool IsStopping() const;

	UFUNCTION(BlueprintPure, Category = "AnimData|LocomotionData", meta = (BlueprintThreadSafe))
	float GetMoveX() const { return MoveX; }

	UFUNCTION(BlueprintPure, Category = "AnimData|LocomotionData", meta = (BlueprintThreadSafe))
	float GetMoveY() const { return MoveY; }
	
protected:
	/** 전투 태그 변경 시 호출되는 콜백 함수 */
	void OnCombatTagChanged(const FGameplayTag InTag, int32 InCount);

	/** 캐릭터 시스템이 완료되었을 때 실행될 초기화 함수 */
	void OnCharacterSystemReady();

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|Combat")
	bool bIsCombatMode = false;
	
	UPROPERTY(EditDefaultsOnly, Category = "AnimData|LocomotionData")
	float PivotLockDuration = 0.3f; // 피벗 감지 후 입력 차단 유지 시간

	
	// ==========================================
	// 게임 스레드 변수 (원본에서 가져올 데이터)
	// ==========================================
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|Cached")
	FVector CachedVelocity = FVector::ZeroVector;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|Cached")
	FVector CachedAcceleration = FVector::ZeroVector;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|Cached")
	FVector CachedFutureVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AnimData|Cached")
	FRotator CachedActorRotation = FRotator::ZeroRotator;
	
	// ==========================================
	// 워커 스레드 변수 (계산에 사용할 데이터, 복사본)
	// ==========================================
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	float MoveX = 0.f;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	float MoveY = 0.f;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	float CurrentAccelerationSize = 0.f;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	FVector CurrentVelocity = FVector::ZeroVector;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	float CurrentVelocitySize = 0.f;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	FVector FutureVelocity = FVector::ZeroVector;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	float FutureVelocitySize = 0.f;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	ECBLocomotionGait CurrentLocomotionGait = ECBLocomotionGait::Run;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	bool bHasAcceleration = false;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	bool bIsPivoting = false;

private:
	/**
	 * 애니메이션 데이터 초기화 함수 (OnCharacterSystemReady 에서 호출)
	 * 캐릭터 시스템이 완료되면 호출할거임.
	 */
	void InitAnimData();

	/** 초기화 여부 (중복 초기화 방지 플래그) */
	bool bIsAnimDataInitialized = false;

	/** [워커 스레드] 이전 로코모션 개이트 */
	ECBLocomotionGait PreviousLocomotionGait = ECBLocomotionGait::Run;

	float WalkMaxSpeed = 250.f;
	float RunMaxSpeed = 430.f;
	float SprintMaxSpeed = 700.f;

	/** [게임 스레드] 피벗 입력 잠금 타이머 */
	float PivotLockTimer = 0.f;
};
