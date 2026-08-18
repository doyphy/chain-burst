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
class UCBHealthBarWidget;

/**
 * 캐릭터의 바디(크기) 셋업.
 * 캡슐 충돌 크기와 스켈레탈 메시 컴포넌트의 상대 트랜스폼을 담는다.
 * 모두 사용하는 메시 모델에 종속되는 값들이라 메시와 한 세트로 로드아웃이 관리한다.
 */
USTRUCT(BlueprintType)
struct FCBBodySetup
{
	GENERATED_BODY()

	// --- 캡슐 (충돌) ---
	/** 캡슐 반지름 */
	UPROPERTY(EditDefaultsOnly, Category = "Body|Capsule")
	float CapsuleRadius = 34.f;

	/** 캡슐 절반 높이 */
	UPROPERTY(EditDefaultsOnly, Category = "Body|Capsule")
	float CapsuleHalfHeight = 88.f;

	// --- 스켈레탈 메시 컴포넌트 상대 트랜스폼 ---
	/** 메시 상대 위치 (보통 Z = -CapsuleHalfHeight 로 발을 캡슐 바닥에 정렬) */
	UPROPERTY(EditDefaultsOnly, Category = "Body|Mesh")
	FVector MeshRelativeLocation = FVector(0.f, 0.f, -88.f);

	/** 메시 상대 회전 (UE 캐릭터 메시는 보통 Yaw -90 으로 전방 정렬) */
	UPROPERTY(EditDefaultsOnly, Category = "Body|Mesh")
	FRotator MeshRelativeRotation = FRotator(0.f, -90.f, 0.f);

	/** 메시 상대 스케일 */
	UPROPERTY(EditDefaultsOnly, Category = "Body|Mesh")
	FVector MeshRelativeScale = FVector(1.f);
};

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
	 * 바디 셋업(캡슐·메시 트랜스폼), 스켈레탈 메쉬·애님BP, 이동 데이터, 액션 몽타주 데이터를 적용한다.
	 * 로드아웃 내부 에셋은 모두 하드 참조라 로드아웃 로드 시점에 함께 resolve되므로 이 함수는 동기 실행된다.
	 * 파생 로드아웃은 이 함수를 오버라이드해 역할별 공용 데이터를 추가로 적용한다 (Super 호출 필수).
	 */
	virtual void ApplyToCharacter(ACBBaseCharacter* InCharacter);

	/**
	 * [공용] 로드아웃이 가진 소프트 참조를 로드해 적용하는 함수 (ApplyToCharacter 의 비동기 꼬리).
	 *
	 * 로드아웃에 소프트 참조가 생기면 여기에 모은다 — "무엇을 더 로드해야 하는지"는 로드아웃이 알고,
	 * "언제 준비 완료인지"는 캐릭터가 정하게 하기 위함이다. 캐릭터는 완료 콜백만 기다리면 된다.
	 *
	 * **어떤 경로로 끝나든 OnComplete 를 반드시 호출할 것.** 빠뜨리면 캐릭터가 영구히 준비 완료되지 않고,
	 * 화면이 검은 채로 멈춘다 → [SystemReady.md]
	 *
	 * 기본 구현은 소프트 참조가 없다고 보고 즉시 완료한다.
	 * @param OnComplete 로드·적용이 끝난 뒤 호출할 콜백
	 */
	virtual void ApplyAsyncToCharacter(ACBBaseCharacter* InCharacter, TFunction<void()> OnComplete);

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

	/** 캐릭터에 등록할 무기 데이터 목록 (단일 무기 = 1개, 쌍수 무기 = 좌/우 2개 등록) */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|Weapons", meta = (TitleProperty = "WeaponSocketType"))
	TArray<TObjectPtr<UCBWeaponData>> WeaponDatas;
	
	// =========================================================
	// 어빌리티 (Abilities) - 역할에 따라 명확히 구분
	// =========================================================
	
	/** 
	 * [액티브] 플레이어/AI가 능동적으로 사용하는 스킬들 
	 * 예: 평타, 스킬 Q, 스킬 E, 필살기
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|Abilities")
	TArray<TSubclassOf<UCBGameplayAbility>> ActiveAbilities;

	/** 
	 * [패시브] 게임 시작 시 부여되어 상시 적용되는 능력
	 * 예: 체력 재생, 이동 속도 증가, 특정 속성 저항
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|Abilities")
	TArray<TSubclassOf<UCBGameplayAbility>> PassiveAbilities;
	
	/** 
	 * [반응형] 특정 조건(피격, 사망, 스턴 등)에서 자동 발동되는 시스템 어빌리티
	 * 예: GA_HitReact(피격 모션 재생), GA_Death(사망 처리), GA_Spawn(등장 연출)
	 * 이들은 입력으로 발동되지 않고, GameplayEvent로 트리거.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|Abilities")
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

	/**
	 * 바디(크기) 셋업 - 캡슐 충돌 크기 + 메시 상대 트랜스폼(위치/회전/스케일).
	 * 스폰 직후엔 캐릭터 생성자의 기본 캡슐이 쓰이고, 로드아웃 로드 후 이 값으로 오버라이드된다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|Visuals")
	FCBBodySetup BodySetup;

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
	// UI - 캐릭터 부착형 UI 위젯 클래스
	// =========================================================

	/** 머리 위 체력바 위젯 클래스 (전 클라이언트에서 필요. UCBUIComponent에 주입되어 준비 완료 후 생성됨) */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|UI")
	TSubclassOf<UCBHealthBarWidget> OverheadHealthBarWidgetClass = nullptr;

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