// project
#include "Characters/CBChaserCharacter.h"
#include "Components/Input/CBInputManagerComponent.h"
#include "DataAssets/Loadout/CBChaserLoadout.h"
#include "Components/Combat/CBChaserCombatComponent.h"
#include "Components/Camera/CBCameraControlComponent.h"
#include "Components/Movement/CBCharacterRotationComponent.h"
#include "Components/Mesh/CBModularMeshComponent.h"
#include "AnimInstances/CBCharacterAnimInstance.h"
#include "PlayerState/CBPlayerState.h"
#include "AssetManager/CBAssetManager.h"

// engine
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"


ACBChaserCharacter::ACBChaserCharacter()
{
	// 캡슐 컴포넌트의 크기를 초기화 (충돌 영역 설정)
	// 스폰 직후 fallback 값. 로드아웃 로드 후 UCBCharacterLoadout의 BodySetup 값으로 오버라이드됨.
	GetCapsuleComponent()->InitCapsuleSize(30.f, 86.0f);

	// 컨트롤러의 회전값을 캐릭터에 직접 적용하지 않도록 설정 (카메라 독립적 회전)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 진영 기본값 (플레이어 = 추격자 진영)
	Team = ECBTeam::Chaser;

	// SpringArmComponent(카메라 붐) 생성 및 루트 컴포넌트에 부착
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComponent->SetupAttachment(RootComponent);
	// 카메라와 캐릭터 사이의 거리 설정
	SpringArmComponent->TargetArmLength = 200.0f;
	// 카메라 위치 오프셋 설정 (Y, Z축으로 이동)
	SpringArmComponent->SocketOffset = FVector(0.0f, 55.0f, 65.0f);
	// 컨트롤러의 회전값을 SpringArm에 적용 (카메라가 마우스/패드 입력에 따라 회전)
	SpringArmComponent->bUsePawnControlRotation = true;

	// 카메라 컴포넌트 생성 및 SpringArm에 부착
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SpringArmComponent, USpringArmComponent::SocketName);
	// 카메라 자체는 컨트롤러 회전을 사용하지 않음 (SpringArm이 회전 제어)
	CameraComponent->bUsePawnControlRotation = false;

	// 캐릭터 이동 시 이동 방향을 바라보도록 설정
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	// 캐릭터 회전 속도 설정 (Yaw 기준)
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// 컴포넌트 생성
	ChaserCombatComponent = CreateDefaultSubobject<UCBChaserCombatComponent>(TEXT("CBChaserCombatComponent"));
	CBCameraControlComponent = CreateDefaultSubobject<UCBCameraControlComponent>(TEXT("CBCameraControlComponent"));
	CBCharacterRotationComponent = CreateDefaultSubobject<UCBCharacterRotationComponent>(TEXT("CBCharacterRotationComponent"));
	CBInputManagerComponent = CreateDefaultSubobject<UCBInputManagerComponent>(TEXT("CBInputManagerComponent"));
	CBModularMeshComponent = CreateDefaultSubobject<UCBModularMeshComponent>(TEXT("CBModularMeshComponent"));
}

/**
 * [서버]에서 실행됨. 새로운 컨트롤러 (플레이어 또는 AI)가 이 캐릭터를 소유할 때 호출되는 함수.
 * @param NewController 새로운 컨트롤러 포인터. 이 컨트롤러가 캐릭터를 소유하게 됨.
 */
void ACBChaserCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitializePlayerSystem();
}

/**
 * [클라이언트]에서 실행됨. PlayerState가 변경될 때 호출되는 함수.
 * 서버에서 PossessedBy()가 호출된 후에 PlayerState가 클라이언트에 복제되면서 이 함수가 호출됨.
 */
void ACBChaserCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitializePlayerSystem();
}

/**
 * 컴포넌트 초기화 후에 호출되는 함수.
 * 컴포넌트가 모두 생성되고 초기화된 후에 설정이 필요한 경우 이 함수에서 처리.
 */
void ACBChaserCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 카메라 컨트롤 컴포넌트 초기화 (스프링암, 카메라 컴포넌트가 필요)
	if (CBCameraControlComponent)
	{
		CBCameraControlComponent->InitializeCamera(SpringArmComponent, CameraComponent);
	}
}

// [공용] 게임 시작 시 호출되는 함수. BeginPlay()는 액터가 게임 세계에 스폰되고 초기화된 후에 한 번만 호출됨.
void ACBChaserCharacter::BeginPlay()
{
	Super::BeginPlay();
}

/**
 * [소유 클라이언트] 폰이 로컬에서 입력을 받게 될 때(빙의/재시작 시) 엔진이 호출하는 함수.
 * @param PlayerInputComponent 캐릭터에 입력 컴포넌트를 설정하는 함수.
 * Enhanced Input 시스템을 사용하여 입력 액션과 태그를 바인딩.
 */
void ACBChaserCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 입력 처리는 입력 매니저 컴포넌트로 위임한다. (바인딩 지연 시도)
	if (CBInputManagerComponent)
	{
		CBInputManagerComponent->SetupPlayerInput(PlayerInputComponent);
	}
}

void ACBChaserCharacter::SetInputConfig(UCBInputConfig* InInputConfig)
{
	// 로드아웃에서 주입된 입력 설정을 입력 매니저 컴포넌트로 전달한다.
	if (CBInputManagerComponent)
	{
		CBInputManagerComponent->SetInputConfig(InInputConfig);
	}
}

bool ACBChaserCharacter::GetCachedAnimInstance(TObjectPtr<UCBCharacterAnimInstance>& OutAnimInstance)
{
	// 캐싱된 CMC가 이미 존재하면 그대로 반환
	if (OutAnimInstance)
	{
		return true;
	}

	// 유효하지 않다면 캐싱 시도
	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		OutAnimInstance = Cast<UCBCharacterAnimInstance>(CharacterMesh->GetAnimInstance());
	}
	
	return (OutAnimInstance != nullptr);
}

// PossessedBy 함수와 OnRep_PlayerState 함수에서 호출되는 초기화 진입 함수.
// 서버와 클라이언트 모두에서 실행됨.
void ACBChaserCharacter::InitializePlayerSystem()
{
	// [공용] 재진입 방지
	if (!StartSystemInitialization())
	{
		return;
	}

	// [공용] ASC/AttributeSet 캐싱 및 ActorInfo 초기화
	InitAbilitySystem();

	// [공용] 로드아웃이 없으면 비동기 작업 없이 즉시 준비 완료 처리
	if (ChaserLoadout.IsNull())
	{
		HandleCharacterSystemReady();
		return;
	}

	// 로드아웃 비동기 로드, InitAbilityActorInfo 완료 후 진입
	UCBAssetManager::Get().LoadAssetAsync<UCBChaserLoadout>(ChaserLoadout, [this](UCBChaserLoadout* LoadedLoadout)
	{
		if (!LoadedLoadout)
		{
			UE_LOG(LogTemp, Error, TEXT("%s 의 로드아웃 로드 실패"), *GetName());

			// 로드아웃이 없으면 이어서 로드할 것도 없음 (영구 잠김 방지)
			HandleCharacterSystemReady();
			return;
		}

		// [공용] 전 인스턴스 공용 데이터 적용 (메쉬·애님BP·이동 데이터·몽타주 데이터)
		LoadedLoadout->ApplyToCharacter(this);

		// [서버] 서버 권위 처리 (어빌리티·무기·이펙트)
		if (HasAuthority())
		{
			Auth_InitServerData(LoadedLoadout);
		}

		// [소유 클라이언트] 소유 클라이언트 처리 (입력 설정)
		if (IsLocallyControlled())
		{
			Local_InitClientData(LoadedLoadout);
		}

		// [공용] 로드아웃의 소프트 참조(기본 의상 파츠 메시 등) 로드
		TWeakObjectPtr<ACBChaserCharacter> WeakThis(this);
		LoadedLoadout->ApplyAsyncToCharacter(this, [WeakThis]()
		{
			// 로드 중 캐릭터가 파괴됐으면 알릴 대상이 없음
			if (ACBChaserCharacter* Character = WeakThis.Get())
			{
				// 성공/실패 무관하게 준비 완료 통지 (영구 잠김 방지)
				Character->HandleCharacterSystemReady();
			}
		});
	});
}

// [공용] ASC/AttributeSet 캐싱 및 ActorInfo 초기화 함수
void ACBChaserCharacter::InitAbilitySystem()
{
	// Player State 가져오기
	ACBPlayerState* PS = GetPlayerState<ACBPlayerState>();
	checkf(PS, TEXT("%s 의 PlayerState 유효하지 않음."), *GetName());

	if (PS)
	{
		// CB ASC 가져오기
		CBASC = PS->GetCBAbilitySystemComponent();
		checkf(CBASC, TEXT("%s 의 AbilitySystemComponent 유효하지 않음."), *GetName());

		// CB AttributeSet 가져오기
		CBAttributeSet = PS->GetCBAttributeSet();
		checkf(CBAttributeSet, TEXT("%s 의 AttributeSet 유효하지 않음."), *GetName());

		if (CBASC)
		{
			// AbilitySystemComponent에 이 캐릭터(Actor)와 소유자 정보를 초기화하여 어빌리티 시스템이 올바르게 동작하도록 설정
			CBASC->InitAbilityActorInfo(PS, this);
		}
	}
}

// [서버] 서버 권위 처리 (어빌리티·무기·이펙트)
void ACBChaserCharacter::Auth_InitServerData(UCBChaserLoadout* InLoadout)
{
	if (!InLoadout) return;

	// 로드아웃에 있는 어빌리티 모두 어빌리티 시스템에 등록
	InLoadout->Auth_GrantAbilitiesToASC(CBASC);
	// 로드아웃에 있는 무기 컴뱃 컴포넌트에 등록
	InLoadout->Auth_RegisterWeaponsToCombatComponent(ChaserCombatComponent);
	// 로드아웃에 있는 이펙트 모두 어빌리티 시스템에 적용
	InLoadout->Auth_ApplyEffectsToASC(CBASC);
}

// [소유 클라이언트] 소유 클라이언트 처리 (입력 설정)
void ACBChaserCharacter::Local_InitClientData(UCBChaserLoadout* InLoadout)
{
	if (!InLoadout) return;

	// 소유 클라이언트 전용 데이터 적용 (입력 설정)
	InLoadout->Local_ApplyToCharacter(this);
}

void ACBChaserCharacter::HandleCharacterSystemReady()
{
	// 베이스 처리 (상태 전환 + 델리게이트 방송 + 어트리뷰트 초기화)
	Super::HandleCharacterSystemReady();

	// 모든 초기화가 완료되었으므로 입력 잠금 해제
	if (CBInputManagerComponent)
	{
		CBInputManagerComponent->SetInputLocked(false);
	}
}

UCBCombatComponent* ACBChaserCharacter::GetCBCombatComponent() const
{
	return ChaserCombatComponent.Get();
}
