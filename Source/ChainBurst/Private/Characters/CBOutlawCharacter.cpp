// project
#include "Characters/CBOutlawCharacter.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem/CBAttributeSet.h"
#include "DataAssets/Loadout/CBOutlawLoadout.h"
#include "Components/Combat/CBOutlawCombatComponent.h"

// engine
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"

ACBOutlawCharacter::ACBOutlawCharacter()
{
	// ASC 생성
	CBASC = CreateDefaultSubobject<UCBAbilitySystemComponent>(TEXT("CBAbilitySystemComponent"));
	// 복제 설정
	CBASC->SetIsReplicated(true);
	// AI 의 복제 설정은 최소한으로 설정
	CBASC->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// AttributeSet 생성
	CBAttributeSet = CreateDefaultSubobject<UCBAttributeSet>(TEXT("CBAttributeSet"));

	// Combat Component 생성
	OutlawCombatComponent = CreateDefaultSubobject<UCBOutlawCombatComponent>(TEXT("CBOutlawCombatComponent"));
	
	// AI 컨트롤러 자동 빙의 설정
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// 캐릭터의 회전이 컨트롤러 회전을 따르지 않음
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 이동 방향으로 회전
	GetCharacterMovement()->bOrientRotationToMovement = true;

	// 컨트롤러 회전 따르지 않음 (기본값 : false, 비전투)
	GetCharacterMovement()->bUseControllerDesiredRotation = false;

	// 회전 속도 설정
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 180.0f, 0.0f);
}

// [서버] 컨트롤러가 이 캐릭터를 소유할 때 호출되는 함수. 어빌리티 시스템 초기화 및 로드아웃 적용 로직 포함.
void ACBOutlawCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (CBASC)
	{
		if (!OutlawLoadout.IsNull())
		{
			// 이미 로드되어 있다면 즉시 사용
			if (OutlawLoadout.IsValid() && CBASC && OutlawCombatComponent)
			{
				OutlawLoadout->Auth_GrantAbilitiesToASC(CBASC);
				OutlawLoadout->Auth_RegisterWeaponsToCombatComponent(OutlawCombatComponent);
				OutlawLoadout->Auth_ApplyEffectsToASC(CBASC);
			}
			else
			{
				// 비동기 로드 요청
				UAssetManager::GetStreamableManager().RequestAsyncLoad(
					OutlawLoadout.ToSoftObjectPath(),
					FStreamableDelegate::CreateWeakLambda(this, [this]()
					{
						if (OutlawLoadout.IsValid() && CBASC && OutlawCombatComponent)
						{
							OutlawLoadout->Auth_GrantAbilitiesToASC(CBASC);
							OutlawLoadout->Auth_RegisterWeaponsToCombatComponent(OutlawCombatComponent);
							OutlawLoadout->Auth_ApplyEffectsToASC(CBASC);
						}
					})
				);
			}
		}
		// AbilitySystemComponent에 이 캐릭터(Actor)와 소유자 정보를 초기화하여 어빌리티 시스템이 올바르게 동작하도록 설정
		CBASC->InitAbilityActorInfo(this, this);
	}
}

UCBCombatComponent* ACBOutlawCharacter::GetCBCombatComponent() const
{
	return OutlawCombatComponent.Get();
}
