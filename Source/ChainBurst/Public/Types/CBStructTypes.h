#pragma once
#include "GameplayTagContainer.h"
#include "Types/CBEnumTypes.h"
#include "CBStructTypes.generated.h"

class ACBBaseWeapon;
class UAnimMontage;

/**
 * 무기 데이터 구조체
 * 무기 태그, 무기 클래스, 무기 전투 타입을 포함
 * CBCombatComponent 에서 사용
 */
USTRUCT(BlueprintType)
struct FCBWeaponData
{
	GENERATED_BODY()

	/** 무기 식별 태그 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag WeaponTag;
	
	/** 무기 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<ACBBaseWeapon> WeaponClass;

	/** 무기 전투 타입 (전투 시 무기 부착, 애니메이션 레이어 선택 등)  */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	ECBWeaponCombatType WeaponCombatType = ECBWeaponCombatType::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChainBurst|Montage")
	TObjectPtr<UAnimMontage> EquipMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChainBurst|Montage")
	TObjectPtr<UAnimMontage> UnequipMontage;
	
	bool IsValid() const;
};


/**
 * 무기 타입별 소켓 부착 정보 구조체
 * CBWeaponSocketData 에서 CombatType 별로 소켓 이름을 직접 입력
 */
USTRUCT(BlueprintType)
struct FCBWeaponSocketConfig
{
	GENERATED_BODY()

	/** 이 설정이 적용될 무기 종류 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	ECBWeaponCombatType WeaponCombatType = ECBWeaponCombatType::None;

	/** 전투 시 부착 소켓 이름 (예: Socket_Combat_Hand_R) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName CombatSocket = NAME_None;

	/** 비전투 시 부착 소켓 이름 (예: Socket_Sheath_Hip_L) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName SheathSocket = NAME_None;

	bool IsValid() const
	{
		return WeaponCombatType != ECBWeaponCombatType::None
			&& CombatSocket != NAME_None;
	}
};