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
 * 무기 태그와 무기 인스턴스를 함께 저장하여 관리
 */
USTRUCT(BlueprintType)
struct FCBRegisteredWeaponData
{
	GENERATED_BODY()

	/** 무기 식별 태그 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag WeaponTag = FGameplayTag::EmptyTag;
	
	/** 스폰된 무기 인스턴스 */
	UPROPERTY()
	TObjectPtr<ACBBaseWeapon> WeaponInstance = nullptr;

	/** 무기 데미지 */
	UPROPERTY()
	float WeaponDamage = 0.0f;

	/** 유효성 검사 함수 */
	bool IsValid() const { return WeaponTag.IsValid() && WeaponInstance != nullptr; };

	FCBRegisteredWeaponData() = default;
	
	FCBRegisteredWeaponData(FGameplayTag InWeaponTag, TObjectPtr<ACBBaseWeapon> InWeaponInstance)
		: WeaponTag(InWeaponTag)
		, WeaponInstance(InWeaponInstance) {}

	FCBRegisteredWeaponData(TObjectPtr<UCBWeaponData> InWeaponData, TObjectPtr<ACBBaseWeapon> InWeaponInstance)
		: WeaponTag(InWeaponData->WeaponTag)
		, WeaponInstance(InWeaponInstance)
		, WeaponDamage(InWeaponData->WeaponDamage) {}
	
	/**
	 * operator== 정의
	 * FCBWeaponData 와 비교 연산
	 */
	bool operator==(const UCBWeaponData* Other) const
	{
		// WeaponTag가 같으면 같은 무기로 취급
		return WeaponTag == Other->WeaponTag;
	}
};

/**
 * [공용] 전투 컴포넌트의 부모 클래스.
 * 무기 장착, 데미지 처리, 전투 상태(태그) 등 추격자와 무법자가 공유하는 로직을 담당.
 */
UCLASS(Abstract)
class CHAINBURST_API UCBCombatComponent : public UCBExtensionComponent
{
	GENERATED_BODY()

public:
	UCBCombatComponent();
	
private:
	/** [저장소] 현재 장착된 무기 데이터 */
	UPROPERTY(Replicated)
	FCBRegisteredWeaponData EquippedWeapon;

	/** 무기 AttackPower GE 핸들 (무기 해제 시 GE 제거에 사용) */
	FActiveGameplayEffectHandle WeaponAttackPowerEffectHandle;

protected:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
public:
	/**
	 * [서버 전용] 무기 태그와 무기 인스턴스를 맵에 등록하는 함수.
	 * UCBCharacterLoadout 에서 호출되어 캐릭터의 무기를 등록하는 데 사용됨.
	 * @param InWeaponToRegister 등록할 무기 데이터.
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Combat")
	void Auth_RegisterWeapon(UCBWeaponData* InWeaponToRegister);

	/**
	 * 현재 장착된 무기가 유효한지 확인하는 함수
	 * @return 현재 장착된 무기가 유효한지 여부 반환 (무기 태그와 인스턴스 모두 유효해야 True 반환)
	 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Combat")
	bool HasValidWeapon() const { return EquippedWeapon.IsValid(); }
	
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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	/**
	 * [서버 전용] 무기를 생성하는 내부 함수
	 * @param WeaponClass 생성할 무기 클래스.
	 * @return 무기 인스턴스 반환, 생성 실패 시 nullptr 반환.
	 */
	ACBBaseWeapon* Auth_SpawnWeapon(TSubclassOf<ACBBaseWeapon> WeaponClass);

	/** [서버 전용] 무기를 파괴하는 내부 함수 */
	void Auth_DestroyWeapon(ACBBaseWeapon* WeaponToDestroy);

	/** [Getter] CachedOwnerMesh */
	USkeletalMeshComponent* GetCachedOwnerMesh();

	/** [Getter] CachedOwnerASC */
	UCBAbilitySystemComponent* GetCachedOwnerASC();
	
	/** 전투 모드로 전환 로직 */
	virtual void OnEnterCombatMode();

	/** 비전투 모드로 전환 로직 */
	virtual void OnExitCombatMode();

	/** [서버 전용] 무기 AttackPower GE를 ASC에 적용하는 함수 */
	void Auth_ApplyWeaponAttackPowerEffect(UCBWeaponData* InWeaponData);

	/** [서버 전용] 무기 AttackPower GE를 ASC에서 제거하는 함수 */
	void Auth_RemoveWeaponAttackPowerEffect();

	/** 트레이스 충돌 결과를 처리하는 함수 */
	void ProcessHit(const FGameplayAbilityTargetDataHandle& TargetDataHandle);

	/**
	 * 캐릭터의 메쉬를 저장
	 * 무기를 부착할 때 자주 호출하기 때문에 캐싱
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<USkeletalMeshComponent> CachedOwnerMesh;

	/** 캐릭터의 ASC를 저장 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UCBAbilitySystemComponent> CachedOwnerASC;
	
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
	
	/** 누적된 히트를 한 번에 처리하는 함수 */
	void FlushPendingHits();

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

	/** 이전 프레임의 무기 뿌리 위치 */
	FVector PrevRootLoc;

	/** 이전 프레임의 무기 끝 위치*/
	FVector PrevTipLoc;
};