// project
#include "Characters/CBChaserCharacter.h"
#include "DataAssets/Input/CBInputConfig.h"
#include "Components/Input/CBInputComponent.h"
#include "CBGameplayTags.h"
#include "DataAssets/Loadout/CBChaserLoadout.h"
#include "Components/Combat/CBChaserCombatComponent.h"
#include "Components/Camera/CBCameraControlComponent.h"
#include "Components/Movement/CBCharacterRotationComponent.h"
#include "AnimInstances/CBCharacterAnimInstance.h"
#include "PlayerState/CBPlayerState.h"
#include "AssetManager/CBAssetManager.h"

// engine
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"


ACBChaserCharacter::ACBChaserCharacter()
{
	// 캡슐 컴포넌트의 크기를 초기화 (충돌 영역 설정)
	GetCapsuleComponent()->InitCapsuleSize(30.f, 86.0f);

	// 컨트롤러의 회전값을 캐릭터에 직접 적용하지 않도록 설정 (카메라 독립적 회전)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

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
 * 컴포넌트 초기화 후에 호출되는 함수. 컴포넌트가 모두 생성되고 초기화된 후에 추가 설정이 필요한 경우 이 함수에서 처리.
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
 * [소유 클라이언트 전용] 폰이 로컬에서 입력을 받게 될 때(빙의/재시작 시) 엔진이 호출하는 함수.
 * @param PlayerInputComponent 캐릭터에 입력 컴포넌트를 설정하는 함수.
 * Enhanced Input 시스템을 사용하여 입력 액션과 태그를 바인딩.
 */
void ACBChaserCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 입력 컴포넌트가 준비된 지금, 나머지 전제조건(InputConfig)이 갖춰졌으면 바인딩되도록 지연 시도만 한다.
	Local_TrySetupInput();
}

void ACBChaserCharacter::Local_TrySetupInput()
{
	// 이미 바인딩이 완료되었으면 종료 (중복 방지)
	if (bInputBindingsSetup) return;

	// 입력 설정(로드아웃 주입)과 입력 컴포넌트(SetupPlayerInputComponent)가 모두 준비되어야 함
	if (!InputConfig || !InputComponent) return;

	// 매핑 컨텍스트 등록을 위해 로컬 플레이어 컨트롤러가 필요
	if (!GetController<APlayerController>()) return;

	// 전제조건 충족 → 실제 바인딩 수행
	Local_SetupInputBindings();
	bInputBindingsSetup = true;
}

void ACBChaserCharacter::Local_SetupInputBindings()
{
	// 현재 컨트롤러에서 로컬 플레이어 객체를 가져옴
	ULocalPlayer* LocalPlayer = GetController<APlayerController>()->GetLocalPlayer();

	// 로컬 플레이어의 서브시스템을 가져옴 (Enhanced Input 시스템 사용)
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

	// 서브시스템이 유효한지 확인
	check(Subsystem);

	// 입력 매핑 컨텍스트를 서브시스템에 추가 (우선순위 0)
	Subsystem->AddMappingContext(InputConfig->DefaultMappingContext, 0);

	// 입력 컴포넌트를 UCBInputComponent 타입으로 캐스팅 (실패 시 에디터에서 에러 발생)
	UCBInputComponent* CBInputComponent = CastChecked<UCBInputComponent>(InputComponent);

	// Input 태그에 해당하는 Input Action을 바인딩
	CBInputComponent->BindNativeInputAction(InputConfig, CBGameplayTags::Input_Action_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
	CBInputComponent->BindNativeInputAction(InputConfig, CBGameplayTags::Input_Action_Look, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
	CBInputComponent->BindNativeInputAction(InputConfig, CBGameplayTags::Input_Action_Camera_Zoom, ETriggerEvent::Triggered, this, &ThisClass::Input_Camera_Zoom);

	// InputConfig 의 Ability Input Actions 배열의 모든 액션을 바인딩 (Input_AbilityInputPressed, Input_AbilityInputReleased 함수와 연결)
	CBInputComponent->BindAbilityInputAction(InputConfig, this, &ThisClass::Input_AbilityInputPressed, &ThisClass::Input_AbilityInputReleased);
}

void ACBChaserCharacter::SetInputConfig(UCBInputConfig* InInputConfig)
{
	InputConfig = InInputConfig;

	// 입력 설정이 주입되었으니 나머지 전제조건이 갖춰졌으면 바인딩되도록 지연 시도
	Local_TrySetupInput();
}

void ACBChaserCharacter::Input_Move(const FInputActionValue& InputActionValue)
{
	// 입력 잠금	여부 확인
	if (bIsInputLocked) return;

	// 피벗 중이면 이동 입력 무시
	// if (GetCachedAnimInstance(CachedAnimInstance) && CachedAnimInstance->IsInputLocked()) return;
	
	const FVector2D MovementVector = InputActionValue.Get<FVector2D>();
	if (Controller != nullptr)
	{
		// Yaw 회전값을 기준으로 전방 및 우측 방향 벡터 계산
		const FRotator YawRotation(0, Controller->GetControlRotation().Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		
		// 입력된 이동 벡터를 기반으로 캐릭터 이동
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ACBChaserCharacter::Input_Look(const FInputActionValue& InputActionValue)
{
	const FVector2D LookAxisVector = InputActionValue.Get<FVector2D>();
	
	if (CBCameraControlComponent)
	{
		CBCameraControlComponent->Input_Look(LookAxisVector);
	}
}

void ACBChaserCharacter::Input_Camera_Zoom(const FInputActionValue& InputActionValue)
{
	const float WheelValue = InputActionValue.Get<float>();
	
	if (CBCameraControlComponent)
	{
		CBCameraControlComponent->Input_Camera_Zoom(WheelValue);
	}
}

void ACBChaserCharacter::Input_AbilityInputPressed(FGameplayTag InInputTag)
{
	CBASC->OnAbilityInputPressed(InInputTag);
}

void ACBChaserCharacter::Input_AbilityInputReleased(FGameplayTag InInputTag)
{
	CBASC->OnAbilityInputReleased(InInputTag);
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
	// ==========================================
	// 공통 로직 (Common)
	// ==========================================
	InitCommonData();

	// ==========================================
	// 서버 로직 (Authority Only)
	// ==========================================
	if (HasAuthority())
	{
		Auth_InitServerData();
	}

	// ==========================================
	// 로컬 클라이언트 로직 (Local Client Only)
	// ==========================================
	if (IsLocallyControlled())
	{
		Local_InitClientData();
	}

	// 캐릭터 시스템이 완료되었음을 알림.
	HandleCharacterSystemReady();

	// 입력 잠금 해제
	bIsInputLocked = false;
}

void ACBChaserCharacter::InitCommonData()
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

	// 전 인스턴스(서버·소유 클라·시뮬 프록시) 공용 데이터 적용 (메쉬/이동/몽타주)
	// 어빌리티/무기/이펙트 등 서버 권위 처리는 Auth_InitServerData 에서 별도 수행
	if (!ChaserLoadout.IsNull())
	{
		UCBAssetManager::Get().LoadAssetAsync<UCBChaserLoadout>(ChaserLoadout, [this](UCBChaserLoadout* LoadedLoadout)
		{
			if (LoadedLoadout)
			{
				// 전 인스턴스 공용 데이터 적용 (메쉬·애님BP·이동 데이터·몽타주 데이터)
				LoadedLoadout->ApplyToCharacter(this);
			}
		});
	}
}

void ACBChaserCharacter::Auth_InitServerData()
{
	// CharacterLoadout 가져오기 및 유효성 검사
	ensureMsgf(!ChaserLoadout.IsNull(), TEXT("%s 의 CharacterLoadout 유효하지 않음."), *GetName());

	// 비동기 로드
	UCBAssetManager::Get().LoadAssetAsync<UCBCharacterLoadout>(ChaserLoadout, [this](UCBCharacterLoadout* LoadedLoadout)
	{
		if (LoadedLoadout)
		{
			// 로드아웃에 있는 어빌리티 모두 어빌리티 시스템에 등록
			LoadedLoadout->Auth_GrantAbilitiesToASC(CBASC);
			// 로드아웃에 있는 무기 컴뱃 컴포넌트에 등록
			LoadedLoadout->Auth_RegisterWeaponsToCombatComponent(ChaserCombatComponent);
			// 로드아웃에 있는 이펙트 모두 어빌리티 시스템에 적용
			LoadedLoadout->Auth_ApplyEffectsToASC(CBASC);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s 의 캐릭터 로드아웃 로드 실패"), *GetName());
		}
	});
}

void ACBChaserCharacter::Local_InitClientData()
{
	// 로드아웃 소프트 참조 유효성 검사
	if (ChaserLoadout.IsNull()) return;

	// 비동기 로드
	UCBAssetManager::Get().LoadAssetAsync<UCBChaserLoadout>(ChaserLoadout, [this](UCBChaserLoadout* LoadedLoadout)
	{
		// 소유 클라이언트 전용 데이터 적용 (입력 설정)
		if (LoadedLoadout)
		{
			LoadedLoadout->Local_ApplyToCharacter(this);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s 의 캐릭터 로드아웃 로드 실패"), *GetName());
		}
	});
}

UCBCombatComponent* ACBChaserCharacter::GetCBCombatComponent() const
{
	return ChaserCombatComponent.Get();
}
