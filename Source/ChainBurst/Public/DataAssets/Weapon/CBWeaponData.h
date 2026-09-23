#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Types/CBEnumTypes.h"
#include "CBWeaponData.generated.h"

class ACBBaseWeapon;
class UGameplayEffect;

/**
 * 무기 하나의 실질 데이터를 정의하는 데이터 에셋
 * (스폰할 무기 클래스, 점유 소켓 타입, 데미지, 피격 연출 큐 태그, 공격력 GE)
 * 로드아웃에 등록되어 캐릭터 초기화 시 컴뱃 컴포넌트로 전달되고,
 * 컴뱃 컴포넌트가 이 데이터를 기반으로 무기(ACBBaseWeapon)를 스폰·등록·관리한다
 */
UCLASS()
class CHAINBURST_API UCBWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 필수 데이터 유효성 검사 함수 */
	bool HasValidData();

	/**
	 * 무기 부착 소켓 타입 (이 무기가 점유하는 소켓 슬롯)
	 * 등록 중복 검사 기준으로 사용. None = 소켓 미점유(소환형 무기 등)
	 * 무기 BP(ACBBaseWeapon)의 WeaponSocketType 과 일치해야 함 (등록 시 불일치 경고)
	 */
	UPROPERTY(EditDefaultsOnly)
	ECBWeaponSocketType WeaponSocketType = ECBWeaponSocketType::None;

	/** 무기 클래스 */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<ACBBaseWeapon> WeaponClass;

	/** 무기 데미지 */
	UPROPERTY(EditDefaultsOnly)
	float WeaponDamage;

	/**
	 * 이 무기에 맞았을 때 재생할 피격 연출 큐 태그.
	 * 공격 어빌리티가 히트 시점에 피격자 ASC 로 실행함. (비우면 피격 연출이 아예 나오지 않으므로 반드시 채울 것)
	 */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "GameplayCue.Combat.HitReact"))
	FGameplayTag HitCueTag;

	/** 무기 공격력 적용 이펙트 */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> WeaponAttackPowerEffect;
};
