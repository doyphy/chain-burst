// project
#include "DataAssets/Loadout/CBCharacterLoadout.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/CBGameplayAbility.h"
#include "Components/Combat/CBCombatComponent.h"
#include "Components/Animation/CBActionComponent.h"
#include "Characters/CBBaseCharacter.h"
#include "DataAssets/Weapon/CBWeaponData.h"

// engine
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"


void UCBCharacterLoadout::ApplyToCharacter(ACBBaseCharacter* InCharacter)
{
	// 캐릭터 유효성 검사
	if (!InCharacter) return;

	// 이동 데이터 캐릭터에 적용 (하드 참조라 로드아웃 로드 시점에 이미 resolve됨)
	InCharacter->SetMovementDataAsset(MovementData);

	// 액션 몽타주 데이터를 액션 컴포넌트에 적용
	if (UCBActionComponent* ActionComponent = InCharacter->GetCBActionComponent())
	{
		ActionComponent->SetMontageData(MontageData);
	}

	// 바디 셋업 적용 (캡슐 충돌 크기 + 메시 상대 트랜스폼). 메시 애셋과 무관하므로 항상 적용한다.
	if (UCapsuleComponent* Capsule = InCharacter->GetCapsuleComponent())
	{
		Capsule->SetCapsuleSize(BodySetup.CapsuleRadius, BodySetup.CapsuleHalfHeight);
	}
	if (USkeletalMeshComponent* MeshComp = InCharacter->GetMesh())
	{
		MeshComp->SetRelativeLocationAndRotation(BodySetup.MeshRelativeLocation, BodySetup.MeshRelativeRotation);
		MeshComp->SetRelativeScale3D(BodySetup.MeshRelativeScale);
	}

	// 스켈레탈 메시 유효성 검사 (하드 참조라 로드아웃 로드 시점에 이미 resolve됨)
	if (!SkeletalMesh) return;

	// 캐릭터 메시 설정
	InCharacter->GetMesh()->SetSkeletalMesh(SkeletalMesh);

	// 캐릭터 메시의 애니메이션 블루프린트 클래스 적용
	if (AnimInstanceClass)
	{
		InCharacter->GetMesh()->SetAnimInstanceClass(AnimInstanceClass);
	}
}

void UCBCharacterLoadout::Auth_GrantAbilitiesToASC(UCBAbilitySystemComponent* InASCToGive, int32 ApplyLevel)
{
	// 어빌리티 시스템 컴포넌트가 유효한지 확인
	check(InASCToGive);
	
	// 액티브 어빌리티 부여
	GrantAbilities(ActiveAbilities, InASCToGive, ApplyLevel);
	// 패시브 어빌리티 부여
	GrantAbilities(PassiveAbilities, InASCToGive, ApplyLevel);
	// 반응형 어빌리티 부여
	GrantAbilities(ReactiveAbilities, InASCToGive, ApplyLevel);
}

void UCBCharacterLoadout::Auth_RegisterWeaponsToCombatComponent(UCBCombatComponent* InCombatComponent)
{
	check(InCombatComponent);

	// 무기 데이터 유효성 검사 (하드 참조라 로드아웃 로드 시점에 이미 resolve됨)
	if (WeaponData && WeaponData->HasValidData())
	{
		InCombatComponent->Auth_RegisterWeapon(WeaponData);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("캐릭터 로드아웃에 등록된 무기 데이터가 유효하지 않음"));
	}
}

void UCBCharacterLoadout::Auth_ApplyEffectsToASC(UCBAbilitySystemComponent* InASCToGive, int32 ApplyLevel)
{
	// ASC 및 GE 배열 유효성 검사
	if (InASCToGive && !StartupEffects.IsEmpty())
	{
		// GE 배열 순회
		for (const auto& EffectClass : StartupEffects)
		{
			// 각 GE 유효성 검사
			if (EffectClass)
			{
				// Context 생성
				FGameplayEffectContextHandle EffectContext = InASCToGive->MakeEffectContext();
				EffectContext.AddSourceObject(this);

				// SpecHandle 생성
				FGameplayEffectSpecHandle SpecHandle = InASCToGive->MakeOutgoingSpec(EffectClass, ApplyLevel, EffectContext);

				// SpecHandle 유효성 검사
				if (SpecHandle.IsValid())
				{
					// GE를 ASC에 적용 (자신에게 적용)
					InASCToGive->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
				}
			}
		}
	}
}

void UCBCharacterLoadout::GrantAbilities(const TArray<TSubclassOf<UCBGameplayAbility>>& InAbilitiesToGive,
                                         UCBAbilitySystemComponent* InASCToGive, int32 ApplyLevel)
{
	// 부여할 어빌리티가 없으면 함수 종료
	if (InAbilitiesToGive.IsEmpty()) return;
	
	// 배열을 순회하며 각 어빌리티를 부여
	for (const auto& Ability : InAbilitiesToGive)
	{
		// 어빌리티 클래스가 유효하지 않으면 건너뜀
		if (!Ability) continue;
		// 어빌리티 스펙 생성
		FGameplayAbilitySpec AbilitySpec(Ability);
		// 어빌리티의 소스 오브젝트를 현재 어빌리티 시스템 컴포넌트의 아바타로 지정
		AbilitySpec.SourceObject = InASCToGive->GetAvatarActor();
		// 어빌리티 레벨 설정
		AbilitySpec.Level = ApplyLevel;

		// 어빌리티 시스템 컴포넌트에 어빌리티 부여
		// 어빌리티의 활성화 정책이 OnGiven 이면 부여 즉시 TryActivateAbility 호출함 (CBGameplayAbility 참고)
		InASCToGive->GiveAbility(AbilitySpec);
	}
}
