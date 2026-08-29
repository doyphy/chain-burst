#pragma once

#include "CoreMinimal.h"
#include "Components/CBExtensionComponent.h"
#include "GameplayTagContainer.h"
#include "DataAssets/Weapon/CBWeaponData.h"
#include "ActiveGameplayEffectHandle.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "CBCombatComponent.generated.h"

class ACBBaseWeapon;
class USkeletalMeshComponent;
class UCBAbilitySystemComponent;

/**
 * 실제 무기 인스턴스와 관련된 데이터를 저장하기 위한 구조체
 * 점유 소켓 타입과 무기 인스턴스를 함께 저장하여 관리
 */
USTRUCT(BlueprintType)
struct FCBRegisteredWeaponData
{
	GENERATED_BODY()

	/** 무기 부착 소켓 타입 (점유 슬롯, 등록 중복 검사 기준. None = 소켓 미점유) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	ECBWeaponSocketType WeaponSocketType = ECBWeaponSocketType::None;

	/** 스폰된 무기 인스턴스 */
	UPROPERTY()
	TObjectPtr<ACBBaseWeapon> WeaponInstance = nullptr;

	/** 무기 데미지 */
	UPROPERTY()
	float WeaponDamage = 0.0f;

	/** 유효성 검사 함수 (인스턴스가 등록의 본질, 소켓 타입은 None 이 정상 값일 수 있어 제외) */
	bool IsValid() const { return WeaponInstance != nullptr; };

	FCBRegisteredWeaponData() = default;

	FCBRegisteredWeaponData(TObjectPtr<UCBWeaponData> InWeaponData, TObjectPtr<ACBBaseWeapon> InWeaponInstance)
		: WeaponSocketType(InWeaponData->WeaponSocketType)
		, WeaponInstance(InWeaponInstance)
		, WeaponDamage(InWeaponData->WeaponDamage) {}
};

/**
 * [공용] 전투 컴포넌트의 부모 클래스.
 * 무기 장착, 데미지 처리, 전투 상태(태그) 등 공통 로직을 담당.
 * 콤보 카운트 및 초기화 관리 담당
 */
UCLASS(Abstract)
class CHAINBURST_API UCBCombatComponent : public UCBExtensionComponent
{
	GENERATED_BODY()

public:
	UCBCombatComponent();

#pragma region Core
	/** 컴포넌트 수명 주기 / 복제 / 오너 캐싱 */
protected:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * 컴포넌트가 끝날 때 호출됨. (캐릭터 파괴·맵 전환·종료)
	 * 스폰한 무기는 부착·소유 어느 쪽으로도 함께 파괴되지 않으므로 여기서 직접 정리함.
	 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** [Getter] CachedOwnerMesh */
	USkeletalMeshComponent* GetCachedOwnerMesh();

	/** [Getter] CachedOwnerASC */
	UCBAbilitySystemComponent* GetCachedOwnerASC();

	/**
	 * 캐릭터의 메쉬를 저장
	 * 무기를 부착할 때 자주 호출하기 때문에 캐싱
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<USkeletalMeshComponent> CachedOwnerMesh;

	/** 캐릭터의 ASC를 저장 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UCBAbilitySystemComponent> CachedOwnerASC;
#pragma endregion

#pragma region Weapon
	/** 무기 등록 / 스폰·파괴 / AttackPower GE */
public:
	/**
	 * [서버 전용] 무기 태그와 무기 인스턴스를 맵에 등록하는 함수.
	 * UCBCharacterLoadout 에서 호출되어 캐릭터의 무기를 등록하는 데 사용됨.
	 * @param InWeaponToRegister 등록할 무기 데이터.
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Combat")
	void Auth_RegisterWeapon(UCBWeaponData* InWeaponToRegister);

	/**
	 * 현재 장착된 무기가 하나라도 유효한지 확인하는 함수
	 * @return 유효한 무기가 하나 이상 있으면 True 반환 (쌍수 무기는 2개 모두 별개로 등록됨)
	 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Combat")
	bool HasValidWeapon() const;

protected:
	/**
	 * [서버 전용] 무기를 생성하는 내부 함수
	 * @param WeaponClass 생성할 무기 클래스.
	 * @return 무기 인스턴스 반환, 생성 실패 시 nullptr 반환.
	 */
	ACBBaseWeapon* Auth_SpawnWeapon(TSubclassOf<ACBBaseWeapon> WeaponClass);

	/**
	 * [서버 전용] 무기를 파괴하는 내부 함수.
	 * 복제 액터라 서버에서 파괴하면 전 클라이언트에서도 사라짐.
	 */
	void Auth_DestroyWeapon(ACBBaseWeapon* WeaponToDestroy);

	/**
	 * [서버 전용] 등록된 무기를 전부 파괴하고 목록을 비우는 함수.
	 * 무기 해제(GE 제거 등)와는 별개로, 무기 인스턴스의 수명만 정리함.
	 */
	void Auth_DestroyAllWeapons();

	/** [서버 전용] 무기 AttackPower GE를 ASC에 적용하는 함수 */
	void Auth_ApplyWeaponAttackPowerEffect(UCBWeaponData* InWeaponData);

	/** [서버 전용] 무기 AttackPower GE를 ASC에서 제거하는 함수 */
	void Auth_RemoveWeaponAttackPowerEffect();

	/** 등록 가능한 최대 무기 수 (쌍수 무기 = 2) */
	static constexpr int32 MaxWeaponCount = 2;

private:
	/** [저장소] 현재 장착된 무기 데이터 목록 (단일 무기는 1개, 쌍수 무기는 2개) */
	UPROPERTY(Replicated)
	TArray<FCBRegisteredWeaponData> EquippedWeapons;

	/** 무기 AttackPower GE 핸들 목록 (무기 해제 시 GE 제거에 사용, 무기별 1개) */
	TArray<FActiveGameplayEffectHandle> WeaponAttackPowerEffectHandles;
#pragma endregion

#pragma region CombatState
	/** 전투 모드(태그) 진입/이탈 */
public:
	/**
	 * [Getter] 현재 전투 상태 확인 (태그 검사)
	 * @return 전투 상태면 True, 비전투 상태면 False 반환
	 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Combat")
	bool IsCombatMode();

	/**
	 * [Setter] 전투 상태 변경 함수
	 * @param bInCombat true면 전투 상태, false면 비전투 상태
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Combat")
	void SetCombatMode(bool bInCombat);

protected:
	/** 전투 모드로 전환 로직 */
	virtual void OnEnterCombatMode();

	/** 비전투 모드로 전환 로직 */
	virtual void OnExitCombatMode();
#pragma endregion

#pragma region WeaponTrace
	/** 무기 트레이스 / 히트 감지·배칭·검증 */
public:
	/** 무기 트레이스 시작 함수 */
	void StartWeaponTrace();

	/** 무기 트레이스 틱 함수 */
	void TickWeaponTrace();

	/** 무기 트레이스 종료 함수 */
	void StopWeaponTrace();

	/** [Server RPC] 로컬에서 감지한 히트 정보를 서버로 전달 (검증 포함) */
	UFUNCTION(Server, Reliable)
	void Server_NotifyAttackHit(const FGameplayAbilityTargetDataHandle& TargetDataHandle);

protected:
	/** 트레이스 충돌 결과를 처리하는 함수 */
	void ProcessHit(const FGameplayAbilityTargetDataHandle& TargetDataHandle);

	/** 누적된 히트를 한 번에 처리하는 함수 */
	void FlushPendingHits();

	/** 트레이스 범위 (구형 트레이스 반지름) */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Trace")
	float TraceRadius = 20.0f;

	/** 트레이스 채널 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Trace")
	TEnumAsByte<ETraceTypeQuery> WeaponTraceChannel = UEngineTypes::ConvertToTraceType(ECC_Pawn);

	/** 트레이스 디버그 표시 여부 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Trace")
	bool bShowDebugTrace = false;

	// 트레이스 분할 개수 (기본값 : 3)
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Trace")
	int32 TraceSubdivisions = 3;

private:
	/** 트레이스 충돌 결과 저장 배열 (중복 방지) */
	UPROPERTY()
	TArray<AActor*> AlreadyHitActors;

	/** 배칭 대기 중인 히트 목록 */
	TArray<FHitResult> PendingHits;

	/** 히트 배칭 타이머 누적값 */
	float HitBatchAccumulator = 0.0f;

	/** 히트 배칭 간격 (초) */
	static constexpr float HitBatchInterval = 0.1f;

	/** 서버 히트 검증 허용 거리 보정값 (레이턴시 보상) */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Validation")
	float HitValidationTolerance = 100.0f;

	/** 현재 트레이스 활성화 여부 */
	bool bIsTracing = false;

	/** 이전 프레임의 무기 뿌리 위치 (EquippedWeapons 와 인덱스 대응) */
	TArray<FVector> PrevRootLocs;

	/** 이전 프레임의 무기 끝 위치 (EquippedWeapons 와 인덱스 대응) */
	TArray<FVector> PrevTipLocs;
#pragma endregion

#pragma region Combo
	/**
	 * 콤보 (서버 권위 + 소유 클라 예측).
	 */
public:
	/**
	 * 콤보를 한 단계 전진시키고 이번에 재생할 인덱스를 반환.
	 * 다른 액션 태그로 전환되거나 인덱스가 최대치 이상이면 리셋 후 진행.
	 * @param InActionTag    콤보 액션 태그
	 * @param MaxComboCount  이 액션의 최대 콤보 수(몽타주 개수). 인덱스가 이 값 이상이면 리셋.
	 * @param InPredictionKey 이 전진을 일으킨 활성화의 예측 키. 거부 시 RollbackCombo가 대조에 사용.
	 * @return 이번에 재생할 콤보 인덱스
	 */
	int32 AdvanceCombo(const FGameplayTag& InActionTag, int32 MaxComboCount, int32 InPredictionKey);

	/**
	 * 예측이 거부됐을 때 콤보를 전진 직전 상태로 되돌린다 (소유 클라 전용).
	 * 0으로 리셋하면 안 된다 — 서버는 전진 직전 값에 머물러 있으므로 방향만 바뀐 채 계속 어긋난다.
	 * @param InPredictionKey 거부된 활성화의 예측 키. 스냅샷을 만든 활성화와 다르면 되돌리지 않는다.
	 * @return 실제로 되돌렸으면 true
	 */
	bool RollbackCombo(int32 InPredictionKey);

	/** 콤보 인덱스·액션 태그 초기화 (어빌리티 종료/캔슬 시 호출) */
	void ResetCombo();

	/** 현재 콤보 인덱스 반환 */
	int32 GetCurrentComboIndex() const { return CurrentComboIndex; }

private:
	/** 현재 콤보 단계 (0부터 시작) */
	int32 CurrentComboIndex = 0;

	/** 현재 콤보가 진행 중인 액션 태그 (태그 전환 시 리셋 판단용) */
	FGameplayTag CurrentComboActionTag;

	/** AdvanceCombo() 직전 상태 (예측 거부 시 되돌리기용) */
	int32 PrevComboIndex = 0;
	FGameplayTag PrevComboActionTag;

	/** 스냅샷을 만든 활성화의 예측 키 (0이면 스냅샷 없음/서버) */
	int32 PrevComboPredictionKey = 0;
#pragma endregion
};
