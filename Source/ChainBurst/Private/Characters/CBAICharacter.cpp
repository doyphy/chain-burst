// project
#include "Characters/CBAICharacter.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem/CBAttributeSet.h"
#include "Components/Combat/CBCombatComponent.h"
#include "DataAssets/Loadout/CBAILoadout.h"
#include "AssetManager/CBAssetManager.h"

// engine
#include "GameFramework/CharacterMovementComponent.h"

ACBAICharacter::ACBAICharacter()
{
	// ASC 생성 (AI는 캐릭터 자체가 소유)
	CBASC = CreateDefaultSubobject<UCBAbilitySystemComponent>(TEXT("CBAbilitySystemComponent"));
	CBASC->SetIsReplicated(true);
	
	// AI 의 복제 설정은 최소한으로 설정
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
	// 컨트롤러 회전 따르지 않음 (기본값 : false, 비전투)
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	// 회전 속도 설정
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 180.0f, 0.0f);

	// 경로 추종(MoveTo)을 가속 기반으로 구동 (기본값 false = 속도 직접 세팅).
	if (FNavMovementProperties* NavMovementProps = GetCharacterMovement()->GetNavMovementProperties())
	{
		NavMovementProps->bUseAccelerationForPaths = true;
	}
}

// [공용] 게임 시작 시 호출되는 함수.
void ACBAICharacter::BeginPlay()
{
	Super::BeginPlay();

	// 서버가 아닐 때
	// 서버는 PossessedBy 함수를 통해 초기화 함수 진입함 (서버에서 중복 호출 방지)
	if (!HasAuthority())
	{
		// 초기화 함수 진입
		InitializeAISystem();
	}
}

// [서버] 컨트롤러가 이 캐릭터를 소유할 때 호출. 빙의 완료 후 초기화를 구동한다.
void ACBAICharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버 초기화 진입 (ASC ActorInfo 초기화 + 공용/서버 권위 데이터 적용)
	InitializeAISystem();
}

// [공용] 초기화 함수, 공용, 로컬, 서버별 초기화 작업 진행
void ACBAICharacter::InitializeAISystem()
{
	// 재진입 방지
	if (!StartSystemInitialization()) return;

	// [공용] ASC의 ActorInfo 초기화
	if (CBASC)
	{
		CBASC->InitAbilityActorInfo(this, this);
	}

	// 로드아웃이 없으면 비동기 작업 없이 즉시 준비 완료 처리
	const TSoftObjectPtr<UCBAILoadout> LoadoutPtr = GetAILoadout();
	if (LoadoutPtr.IsNull())
	{
		HandleCharacterSystemReady();
		return;
	}

	// [비동기] 로드아웃 로드하고, 콜백에서 공용 데이터(전 인스턴스) + 서버 권위 데이터(서버)를 적용.
	UCBAssetManager::Get().LoadAssetAsync<UCBAILoadout>(LoadoutPtr, [this](UCBAILoadout* LoadedLoadout)
	{
		if (LoadedLoadout)
		{
			// [공용] 전 인스턴스 공용 데이터 적용 (메쉬·애님BP·이동 데이터·몽타주 데이터)
			LoadedLoadout->ApplyToCharacter(this);

			// [서버] 서버 권위 데이터 적용 (어빌리티·무기·이펙트). PossessedBy에서 InitAbilityActorInfo 완료 후 진입하므로 안전.
			if (HasAuthority())
			{
				LoadedLoadout->Auth_GrantAbilitiesToASC(CBASC);

				// [서버] 컴뱃 컴포넌트를 가진 AI만 무기 등록. 없는 AI는 스킵.
				if (UCBCombatComponent* CombatComponent = GetCBCombatComponent())
				{
					LoadedLoadout->Auth_RegisterWeaponsToCombatComponent(CombatComponent);
				}

				LoadedLoadout->Auth_ApplyEffectsToASC(CBASC);

				// [서버] AI 두뇌(BT)를 컨트롤러에 주입. 컨트롤러가 서버 전용이라 서버에서만 필요.
				LoadedLoadout->Auth_ApplyBehaviorTreeToController(GetController());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s 의 AI 로드아웃 로드 실패"), *GetName());
		}

		// 성공/실패 무관하게 준비 완료 통지 (영구 잠김 방지)
		HandleCharacterSystemReady();
	});
}
