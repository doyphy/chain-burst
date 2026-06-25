#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CBWeaponData.generated.h"

class ACBBaseWeapon;
class UGameplayEffect;

UCLASS()
class CHAINBURST_API UCBWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 필수 데이터 유효성 검사 함수 */
	bool HasValidData();

	/** 무기 식별 태그 */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "Item"))
	FGameplayTag WeaponTag;
	
	/** 무기 클래스 */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<ACBBaseWeapon> WeaponClass;

	/** 무기 데미지 */
	UPROPERTY(EditDefaultsOnly)
	float WeaponDamage;

	/** 무기 공격력 적용 이펙트 */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> WeaponAttackPowerEffect;
};
