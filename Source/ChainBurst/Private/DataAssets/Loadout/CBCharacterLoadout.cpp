// project
#include "DataAssets/Loadout/CBCharacterLoadout.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/CBGameplayAbility.h"
#include "Components/Combat/CBCombatComponent.h"
#include "DataAssets/Weapon/CBWeaponData.h"
#include "AssetManager/CBAssetManager.h"

// engine
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"


void UCBCharacterLoadout::ApplyMeshToCharacter(ACharacter* InCharacter)
{
	// 캐릭터 유효성 검사
	if (!InCharacter) return;

	// 스켈레탈 메시 소프트 참조 유효성 검사
	if (SkeletalMesh.IsNull()) return;

	// 비동기 로드
	UCBAssetManager::Get().LoadAssetAsync<USkeletalMesh>(SkeletalMesh, [InCharacter, this](USkeletalMesh* LoadedMesh)
	{
		// 로드된 메시와 캐릭터 유효성 검사
		if (!LoadedMesh || !IsValid(InCharacter)) return;

		// 캐릭터 메시 설정
		InCharacter->GetMesh()->SetSkeletalMesh(LoadedMesh);

		// 캐릭터 메시의 애니메이션 블루프린트 클래스 적용
		if (AnimInstanceClass)
		{
			InCharacter->GetMesh()->SetAnimInstanceClass(AnimInstanceClass);
		}
	});
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

	// 소프트 참조 유효성 검사
	if (WeaponData.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("캐릭터 로드아웃에 등록된 무기 데이터가 유효하지 않음"));	
		return;
	}

	// 무기 데이터 비동기 로드
	UCBAssetManager::Get().LoadAssetAsync<UCBWeaponData>(WeaponData, [InCombatComponent](UCBWeaponData* LoadedWeaponData)
	{
		if (LoadedWeaponData && LoadedWeaponData->HasValidData())
		{
			InCombatComponent->Auth_RegisterWeapon(LoadedWeaponData);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("캐릭터 로드아웃의 무기 데이터 로드 실패"));
		}
	});
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
