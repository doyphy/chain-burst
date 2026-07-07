#pragma once

#include "CoreMinimal.h"
#include "AnimInstances/CBBaseAnimInstance.h"
#include "GameplayTagContainer.h"
#include "CBCharacterAnimInstance.generated.h"

class ACBBaseCharacter;
class UCharacterMovementComponent;

UENUM(BlueprintType)
enum class ECBLocomotionGait : uint8
{
	Walk    UMETA(DisplayName = "Walk"),
	Run     UMETA(DisplayName = "Run"),
	Sprint  UMETA(DisplayName = "Sprint")
};

/**
 * 폰이 소유하는 애님 인스턴스의 공통 베이스.
 * Player/AI가 모두 공유하는 로직만 담는다 — 참조 캐싱, 시스템 준비 초기화, 로코모션 데이터(속도/가속) 계산,
 * 개이트 결정, 전투 모드 태그, 몽타주 재생.
 * 애니메이션은 상태 머신으로 구현하며, 상태 전이 판단에 쓰는 데이터를 여기서 제공한다.
 * 시점/입력 종속 로직은 UCBPlayerAnimInstance, AI 상태 종속 로직은 UCBAIAnimInstance로 분리한다.
 */
UCLASS()
class CHAINBURST_API UCBCharacterAnimInstance : public UCBBaseAnimInstance
{
	GENERATED_BODY()

protected:
	//~ Begin UAnimInstance Interface
	/** 게임 스레드에서 실행되는 업데이트 함수 (UObject 접근 가능) */
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	/** 워커 스레드에서 실행되는 업데이트 함수 (UObject 접근 불가 (외부 접근 불가, 내부 데이터로만), 매우 빠름)*/
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	//~ End UAnimInstance Interface

#pragma region References
	/** 참조 캐싱 — 오너 캐릭터/CMC를 지연 캐싱 */
protected:
	UPROPERTY(BlueprintReadOnly, Transient, Category = "References")
	TWeakObjectPtr<ACBBaseCharacter> CachedCharacter = nullptr;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "References")
	TWeakObjectPtr<UCharacterMovementComponent> CachedCMC = nullptr;

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
#pragma endregion

#pragma region Lifecycle
	/** 시스템 준비 초기화 흐름 — ASC 준비 완료 후 실행할 초기화 로직 */
protected:
	/** 캐릭터 시스템이 완료되었을 때 실행될 초기화 함수 (애니메이션 데이터 초기화) */
	virtual void OnCharacterSystemReady() override;

private:
	/**
	 * 애니메이션 데이터 초기화 함수 (OnCharacterSystemReady 에서 1회 호출)
	 * 캐릭터 시스템이 완료되면 호출할거임.
	 */
	void InitAnimData();
#pragma endregion

#pragma region Locomotion
	/** 로코모션 데이터 계산 및 상태 쿼리 (상태 머신 전이 판단용) */
protected:
	/** 기본적인 데이터 처리 함수 (어빌리티, 컴뱃 컴포넌트 불필요) */
	void UpdateBasicMovementData();

public:
	// ==========================================
	// 워커 스레드에서 계산할 함수 (외부에서 호출 가능)
	// ==========================================
	UFUNCTION(BlueprintPure, Category = "AnimData|LocomotionData", meta = (BlueprintThreadSafe))
	bool IsMoving() const;

	UFUNCTION(BlueprintPure, Category = "AnimData|LocomotionData", meta = (BlueprintThreadSafe))
	bool IsStopping() const;

	UFUNCTION(BlueprintPure, Category = "AnimData|LocomotionData", meta = (BlueprintThreadSafe))
	float GetMoveX() const { return MoveX; }

	UFUNCTION(BlueprintPure, Category = "AnimData|LocomotionData", meta = (BlueprintThreadSafe))
	float GetMoveY() const { return MoveY; }

protected:
	// ==========================================
	// 게임 스레드 변수 (원본에서 가져올 데이터)
	// ==========================================
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AnimData|Cached")
	FVector CachedVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AnimData|Cached")
	FVector CachedAcceleration = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AnimData|Cached")
	FRotator CachedActorRotation = FRotator::ZeroRotator;

	// ==========================================
	// 워커 스레드 변수 (계산에 사용할 데이터, 복사본)
	// ==========================================
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AnimData|LocomotionData")
	float MoveX = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AnimData|LocomotionData")
	float MoveY = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AnimData|LocomotionData")
	float CurrentAccelerationSize = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AnimData|LocomotionData")
	FVector CurrentVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AnimData|LocomotionData")
	float CurrentVelocitySize = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AnimData|LocomotionData")
	ECBLocomotionGait CurrentLocomotionGait = ECBLocomotionGait::Run;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AnimData|LocomotionData")
	bool bHasAcceleration = false;
#pragma endregion

#pragma region Combat
	/** 전투 모드 태그 반영 (Status.Combat.InCombat) */
protected:
	/** 커스텀 데이터 처리 함수 (어빌리티, 컴뱃 컴포넌트 필요) */
	void UpdateCombatAndAbilityData();

	/** 전투 태그 변경 시 호출되는 콜백 함수 */
	void OnCombatTagChanged(const FGameplayTag InTag, int32 InCount);

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AnimData|Combat")
	bool bIsCombatMode = false;
#pragma endregion

#pragma region Montage
	/** 몽타주 재생 (UCBActionComponent가 호출) */
public:
	/**
	 * 몽타주 재생하는 함수.
	 * @param InMontage 재생할 몽타주. 액션 컴포넌트에서 태그로 몽타주 검색 후 전달.
	 * @param PlayRate 재생 속도 배율 (1.0 = 기본). 공격 속도 어트리뷰트를 반영해 UCBActionComponent에서 계산 후 전달.
	 */
	void PlayMontage(UAnimMontage* InMontage, float PlayRate = 1.f);
#pragma endregion
};
