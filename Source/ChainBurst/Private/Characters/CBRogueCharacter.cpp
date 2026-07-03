// project
#include "Characters/CBRogueCharacter.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem/CBAttributeSet.h"
#include "DataAssets/Loadout/CBRogueLoadout.h"
#include "DataAssets/Loadout/CBCharacterLoadout.h"
#include "AssetManager/CBAssetManager.h"

// engine
#include "GameFramework/CharacterMovementComponent.h"

ACBRogueCharacter::ACBRogueCharacter()
{
	// ASC 생성
	CBASC = CreateDefaultSubobject<UCBAbilitySystemComponent>(TEXT("CBAbilitySystemComponent"));
	CBASC->SetIsReplicated(true);
	CBASC->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// AttributeSet 생성
	CBAttributeSet = CreateDefaultSubobject<UCBAttributeSet>(TEXT("CBAttributeSet"));

	// AI 컨트롤러 자동 빙의 설정
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// 캐릭터의 회전이 컨트롤러 회전을 따르지 않음
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 이동 방향으로 회전
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 180.0f, 0.0f);
}

// [공용] 서버와 클라이언트 모두 초기화해야 하는 작업
void ACBRogueCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 로드아웃 소프트 참조 유효성 검사
	if (RogueLoadout.IsNull()) return;

	// 비동기 로드
	UCBAssetManager::Get().LoadAssetAsync<UCBRogueLoadout>(RogueLoadout, [this](UCBRogueLoadout* LoadedLoadout)
	{
		// 전 인스턴스 공용 데이터 적용 (메쉬·애님BP·이동 데이터·몽타주 데이터)
		if (LoadedLoadout)
		{
			LoadedLoadout->ApplyToCharacter(this);
		}
	});
}

// [서버] 서버에서 초기화해야 하는 작업
void ACBRogueCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// ASC 유효성 검사
	if (!CBASC) return;

	// ASC 초기화
	CBASC->InitAbilityActorInfo(this, this);

	// 로드아웃 소프트 참조 유효성 검사
	if (RogueLoadout.IsNull()) return;

	// 비동기 로드
	UCBAssetManager::Get().LoadAssetAsync<UCBRogueLoadout>(RogueLoadout, [this](UCBRogueLoadout* LoadedLoadout)
	{
		// 어빌리와 이펙트 적용
		if (LoadedLoadout)
		{
			LoadedLoadout->Auth_GrantAbilitiesToASC(CBASC);
			LoadedLoadout->Auth_ApplyEffectsToASC(CBASC);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s 의 RogueLoadout 로드 실패"), *GetName());
		}
	});
}
