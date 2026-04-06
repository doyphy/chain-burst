#pragma once

#include "CoreMinimal.h"
#include "Components/CBExtensionComponent.h"
#include "GameplayTagContainer.h"
#include "Types/CBStructTypes.h"
#include "CBCombatComponent.generated.h"

class ACBBaseWeapon;
class USkeletalMeshComponent;
class UCBAbilitySystemComponent;

/**
 * 실제 무기 인스턴스와 관련된 데이터를 저장하기 위한 구조체
 * 무기 슬롯 배열에 저장하고 관리함.
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

	/** 무기 전투 타입 */
	UPROPERTY()
	ECBWeaponCombatType WeaponCombatType = ECBWeaponCombatType::None;

	/** 무기 장착 몽타주 **/
	UPROPERTY()
	TObjectPtr<UAnimMontage> EquipMontage = nullptr;

	/** 무기 해제 몽타주 **/
	UPROPERTY()
	TObjectPtr<UAnimMontage> UnequipMontage = nullptr;
	
	/** 유효성 검사 함수 */
	bool IsValid() const { return WeaponTag.IsValid() && WeaponInstance != nullptr; };
	
	FCBRegisteredWeaponData() {}
	
	FCBRegisteredWeaponData(FGameplayTag InWeaponTag, TObjectPtr<ACBBaseWeapon> InWeaponInstance)
		: WeaponTag(InWeaponTag)
		, WeaponInstance(InWeaponInstance) {}
	
	FCBRegisteredWeaponData(FGameplayTag InWeaponTag, TObjectPtr<ACBBaseWeapon> InWeaponInstance, ECBWeaponCombatType InWeaponCombatType)
		: WeaponTag(InWeaponTag)
		, WeaponInstance(InWeaponInstance)
		, WeaponCombatType(InWeaponCombatType) {}

	/** FCBWeaponData 받아서 초기화하는 생성자 */
	FCBRegisteredWeaponData(const FCBWeaponData& InWeaponData, ACBBaseWeapon* InWeaponInstance)
		: WeaponTag(InWeaponData.WeaponTag)
		, WeaponInstance(InWeaponInstance)
		, WeaponCombatType(InWeaponData.WeaponCombatType)
		, EquipMontage(InWeaponData.EquipMontage)
		, UnequipMontage(InWeaponData.UnequipMontage) {}

	/**
	 * operator== 정의
	 * FCBWeaponData 와 비교 연산
	 */
	bool operator==(const FCBWeaponData& Other) const
	{
		// WeaponTag가 같으면 같은 무기로 취급
		return WeaponTag == Other.WeaponTag;
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

private:
	/** [저장소] 장착된 무기 슬롯 리스트 */
	TArray<FCBRegisteredWeaponData> WeaponSlots;

	/**
	 * [상태] 현재 장착된 무기의 인덱스
	 * (0부터 시작, -1은 무기가 없음)
	 */
	int32 CurrentWeaponSlot = INDEX_NONE;

protected:
	/**
	 * 캐릭터의 메쉬를 저장
	 * 무기를 부착할 때 자주 호출하기 때문에 캐싱
	 */
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> CachedOwnerMesh;

	/** 캐릭터의 ASC를 저장 */
	UPROPERTY()
	TObjectPtr<UCBAbilitySystemComponent> CachedOwnerASC;
	
public:
	/**
	 * 무기 태그와 무기 인스턴스를 맵에 등록하는 함수.
	 * UCBCharacterLoadout 에서 호출되어 캐릭터의 무기를 등록하는 데 사용됨.
	 * @param InWeaponToRegister 등록할 무기 데이터.
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Combat")
	void RegisterWeapon(FCBWeaponData InWeaponToRegister);

	/**
	 * [Getter] 현재 전투 상태 확인 (태그 검사)
	 * @return 전투 상태면 True, 비전투 상태면 False 반환
	 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Combat")
	bool IsCombatMode();

	/** [Getter] 현재 무기의 장착 몽타주 반환 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Combat")
	UAnimMontage* GetCurrentEquipMontage() const;

	/** [Getter] 현재 무기의 해제 몽타주 반환 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Combat")
	UAnimMontage* GetCurrentUnequipMontage() const;
	
	/**
	 * [Setter] 전투 상태 변경 함수
	 * @param bInCombat true면 전투 상태, false면 비전투 상태
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Combat")
	void SetCombatMode(bool bInCombat);

protected:
	/**
	 * 무기를 생성하는 내부 함수
	 * @param WeaponClass 생성할 무기 클래스.
	 * @return 무기 인스턴스 반환, 생성 실패 시 nullptr 반환.
	 */
	ACBBaseWeapon* SpawnWeapon(TSubclassOf<ACBBaseWeapon> WeaponClass);

	/** 무기를 파괴하는 내부 함수 */
	void DestroyWeapon(ACBBaseWeapon* WeaponToDestroy);

	/** [Getter] CachedOwnerMesh */
	USkeletalMeshComponent* GetCachedOwnerMesh();

	/** [Getter] CachedOwnerASC */
	UCBAbilitySystemComponent* GetCachedOwnerASC();

	/** [Getter] 현재 장착된 무기의 인덱스 */
	int32 GetCurrentWeaponSlot();

	/** [Setter] 현재 장착된 무기의 인덱스 */
	void SetCurrentWeaponSlot(int32 NewSlot);
	
	/** 전투 모드로 전환 로직 */
	virtual void OnEnterCombatMode();

	/** 비전투 모드로 전환 로직 */
	virtual void OnExitCombatMode();

	/** 무기 교체 함수 */
	void SwapWeapon(int32 NewWeaponSlot);

	/**
	 * 무기 상태 리셋 및 복구 함수 (잘못된 예외 상황이거나 초기화가 필요할 때 사용)
	 * - 모든 무기 (WeaponSlots) 초기화
	 * - 상태 변수 초기화
	 */
	void ResetWeaponState();
};
