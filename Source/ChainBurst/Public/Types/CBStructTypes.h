#pragma once
#include "GameplayTagContainer.h"
#include "Types/CBEnumTypes.h"
#include "CBStructTypes.generated.h"

class ACBBaseWeapon;

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
	
	/** 비전투 시 무기를 부착할 소켓 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	ECBWeaponSheathSocket SheathSocketType = ECBWeaponSheathSocket::None;

	bool IsValid() const;
};
