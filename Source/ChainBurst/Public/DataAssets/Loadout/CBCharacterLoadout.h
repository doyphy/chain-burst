#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CBCharacterLoadout.generated.h"

class UCBGameplayAbility;
class UCBAbilitySystemComponent;
class UCBCombatComponent;
class UGameplayEffect;
class UCBWeaponData;
class UCBCharacterMovementData;
class UCBActionMontageData;
class ACBBaseCharacter;

/**
 * 캐릭터의 공통 데이터
 */
UCLASS()
class CHAINBURST_API UCBCharacterLoadout : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * [공용] 전 인스턴스(서버·클라)에서 필요한 데이터를 캐릭터에 일괄 적용하는 함수
	 * 스켈레탈 메쉬·애님BP, 이동 데이터, 액션 몽타주 데이터를 적용한다.
	 * 로드아웃 내부 에셋은 모두 하드 참조라 로드아웃 로드 시점에 함께 resolve되므로 이 함수는 동기 실행된다.
	 */
	void ApplyToCharacter(ACBBaseCharacter* InCharacter);

	/** [서버 전용] 어빌리티 시스템 컴포넌트에 어빌리티를 부여하는 함수 */
	virtual void Auth_GrantAbilitiesToASC(UCBAbilitySystemComponent* InASCToGive, int32 ApplyLevel = 1);

	/** [서버 전용] 컴뱃 컴포넌트에 무기를 등록하는 함수 */
	virtual void Auth_RegisterWeaponsToCombatComponent(UCBCombatComponent* InCombatComponent);

	/** [서버 전용] 어빌리티 시스템 컴포넌트에 이펙트를 적용하는 함수 */
	virtual void Auth_ApplyEffectsToASC(UCBAbilitySystemComponent* InASCToGive, int32 ApplyLevel = 1);
	
protected:
	// =========================================================
	// 무기 데이터 (Weapons) - 캐릭터에 등록할 무기
	// =========================================================
	UPROPERTY(EditDefaultsOnly, Category = "Loadout", meta = (TitleProperty = "WeaponTag"))
	TObjectPtr<UCBWeaponData> WeaponData = nullptr;
	
	// =========================================================
	// 어빌리티 (Abilities) - 역할에 따라 명확히 구분
	// =========================================================
	
	/** 
	 * [액티브] 플레이어/AI가 능동적으로 사용하는 스킬들 
	 * 예: 평타, 스킬 Q, 스킬 E, 필살기
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout")
	TArray<TSubclassOf<UCBGameplayAbility>> ActiveAbilities;

	/** 
	 * [패시브] 게임 시작 시 부여되어 상시 적용되는 능력
	 * 예: 체력 재생, 이동 속도 증가, 특정 속성 저항
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout")
	TArray<TSubclassOf<UCBGameplayAbility>> PassiveAbilities;
	
	/** 
	 * [반응형] 특정 조건(피격, 사망, 스턴 등)에서 자동 발동되는 시스템 어빌리티
	 * 예: GA_HitReact(피격 모션 재생), GA_Death(사망 처리), GA_Spawn(등장 연출)
	 * 이들은 입력으로 발동되지 않고, GameplayEvent로 트리거.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout")
	TArray<TSubclassOf<UCBGameplayAbility>> ReactiveAbilities;

	// =========================================================
	// 외형 및 연출 (Visuals)
	// =========================================================

	/** 캐릭터의 외형 (스켈레탈 메쉬) */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|Visuals")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	/** 사용할 애니메이션 블루프린트 클래스 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|Visuals")
	TSubclassOf<UAnimInstance> AnimInstanceClass;

	// =========================================================
	// 캐릭터 데이터 (Data) - 전 인스턴스에서 필요한 데이터 에셋
	// =========================================================

	/** 이동 데이터 (속도 등). ACBBaseCharacter에 캐싱되어 애님/어트리뷰트/이동 어빌리티에서 사용 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|Data")
	TObjectPtr<UCBCharacterMovementData> MovementData = nullptr;

	/** 액션 몽타주 데이터. UCBActionComponent에 캐싱되어 태그 기반 몽타주 재생에 사용 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|Data")
	TObjectPtr<UCBActionMontageData> MontageData = nullptr;

	// =========================================================
	// 기본 스탯 (Stats & Attributes)
	// =========================================================

	/** 스탯 초기화를 위한 GameplayEffect (GE_InitStats) */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout")
	TArray<TSubclassOf<UGameplayEffect>> StartupEffects;
	
	// =========================================================
	// 식별자 (Identity)
	// =========================================================
	
	/** 캐릭터를 구분하는 고유 태그 (예: Character.Type.Tracer) */
	
protected:
	/** 전달받은 어빌리티 배열을 어빌리티 시스템 컴포넌트에 부여하는 내부 함수 */
	void GrantAbilities(const TArray<TSubclassOf<UCBGameplayAbility>>& InAbilitiesToGive, UCBAbilitySystemComponent* InASCToGive, int32 ApplyLevel = 1);

	/** 어빌리티 시스템 컴포넌트에 이펙트 적용하는 함수 */
};