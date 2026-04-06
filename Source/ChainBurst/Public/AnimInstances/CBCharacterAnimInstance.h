#pragma once

#include "CoreMinimal.h"
#include "AnimInstances/CBBaseAnimInstance.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "GameplayTagContainer.h"
#include "CBCharacterAnimInstance.generated.h"

class ACBBaseCharacter;
class UCharacterMovementComponent;
class UCBCharacterTrajectoryComponent;

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
	
public:
	//~ Begin UAnimInstance Interface
	virtual void NativeInitializeAnimation() override;
	/** 게임 스레드에서 실행되는 업데이트 함수 */
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	/** 워커 스레드에서 실행되는 업데이트 함수 */
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	//~ End UAnimInstance Interface
	
protected:	
	UPROPERTY(BlueprintReadOnly, Transient, Category = "References")
	TObjectPtr<ACBBaseCharacter> CachedCharacter;
	
	UPROPERTY(BlueprintReadOnly, Transient, Category = "References")
	TObjectPtr<UCharacterMovementComponent> CachedMovementComp;
	
	UPROPERTY(BlueprintReadOnly, Transient, Category = "References")
	TObjectPtr<UCBCharacterTrajectoryComponent> CachedTrajectoryComp;
	
public:
	UFUNCTION(BlueprintPure, Category = "AnimData|LocomotionData", meta = (BlueprintThreadSafe))
	bool IsStarting() const;

	UFUNCTION(BlueprintPure, Category = "AnimData|LocomotionData", meta = (BlueprintThreadSafe))
	bool IsPivoting() const;

	UFUNCTION(BlueprintPure, Category = "AnimData|LocomotionData", meta = (BlueprintThreadSafe))
	bool IsMoving() const;

	UFUNCTION(BlueprintPure, Category = "AnimData|LocomotionData", meta = (BlueprintThreadSafe))
	bool IsStopping() const;

	bool IsInputLocked() const;
	
	UFUNCTION(BlueprintPure, Category = "AnimData|LocomotionData", meta = (BlueprintThreadSafe))
	EPoseSearchInterruptMode CalculatePoseSearchInterruptMode();

protected:
	/** 전투 태그 변경 시 호출되는 콜백 함수 */
	void OnCombatTagChanged(const FGameplayTag InTag, int32 InCount);
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|Combat")
	bool bIsCombatMode = false;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|Cached")
	FVector CachedVelocity = FVector::ZeroVector;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|Cached")
	FVector CachedAcceleration = FVector::ZeroVector;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|Cached")
	FVector CachedFutureVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AnimData|Cached")
	FRotator CachedActorRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	float InputX = 0.f;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	float InputY = 0.f;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	ECBLocomotionState CurrentLocomotionState = ECBLocomotionState::Idle;
	
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
	EPoseSearchInterruptMode CurrentInterruptMode = EPoseSearchInterruptMode::DoNotInterrupt;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	bool bHasAcceleration = false;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	bool bIsPivoting = false;

	UPROPERTY(EditDefaultsOnly, Category = "AnimData|LocomotionData")
	float PivotLockDuration = 0.3f; // 피벗 감지 후 입력 차단 유지 시간
	
private:
	ECBLocomotionGait PreviousLocomotionGait = ECBLocomotionGait::Run;

	float WalkMaxSpeed = 250.f;
	float RunMaxSpeed = 430.f;
	float SprintMaxSpeed = 700.f;

	float PivotLockTimer = 0.f;

public:
	UFUNCTION(BlueprintPure, Category = "AnimData|LocomotionData", meta = (BlueprintThreadSafe))
	float GetInputX() const { return InputX; }

	UFUNCTION(BlueprintPure, Category = "AnimData|LocomotionData", meta = (BlueprintThreadSafe))
	float GetInputY() const { return InputY; }
};
