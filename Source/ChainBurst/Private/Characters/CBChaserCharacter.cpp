// project
#include "Characters/CBChaserCharacter.h"
#include "DataAssets/Input/CBInputConfig.h"
#include "Components/Input/CBInputComponent.h"
#include "CBGameplayTags.h"
#include "DataAssets/Loadout/CBChaserLoadout.h"
#include "Components/Combat/CBChaserCombatComponent.h"
#include "Components/Camera/CBCameraControlComponent.h"

// engine
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

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
	GetCharacterMovement()->bOrientRotationToMovement = true;
	// 캐릭터 회전 속도 설정 (Yaw 기준)
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	// 걷기 최대 속도 설정
	GetCharacterMovement()->MaxWalkSpeed = 500.0f;
	// 걷기 감속(브레이크) 가속도 설정
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;

	CombatComponent = CreateDefaultSubobject<UCBChaserCombatComponent>(TEXT("CombatComponent"));
	CameraControlComponent = CreateDefaultSubobject<UCBCameraControlComponent>(TEXT("CameraControlComponent"));
}

void ACBChaserCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	// CharacterLoadout 가 유효한 소프트 레퍼런스(SoftObjectPtr)인지 확인
	if (!CharacterLoadout.IsNull())
	{
		// 이미 로드되어 있다면 즉시 사용
		if (CharacterLoadout.IsValid() && CBAbilitySystemComponent && CombatComponent)
		{
			// 로드아웃에 있는 어빌리티 모두 어빌리티 시스템에 등록
			CharacterLoadout->GrantAbilitiesToASC(CBAbilitySystemComponent);
			// 로드아웃에 있는 무기 모두 컴뱃 컴포넌트에 등록
			CharacterLoadout->RegisterWeaponsToCombatComponent(CombatComponent);
		}
		else
		{
			// 비동기 로드 요청
			// 로드가 완료되기 전에 먼저 어빌리티를 활성화해도 문제 없음 (TryAbility).
			UAssetManager::GetStreamableManager().RequestAsyncLoad(
				CharacterLoadout.ToSoftObjectPath(),
				FStreamableDelegate::CreateWeakLambda(this, [this]()
				{
					if (CharacterLoadout.IsValid() && CBAbilitySystemComponent && CombatComponent)
					{
						// 로드아웃에 있는 어빌리티 모두 어빌리티 시스템에 등록
						CharacterLoadout->GrantAbilitiesToASC(CBAbilitySystemComponent);
						// 로드아웃에 있는 무기 모두 컴뱃 컴포넌트에 등록
						CharacterLoadout->RegisterWeaponsToCombatComponent(CombatComponent);
					}
				})
			);
		}
	}
}

void ACBChaserCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (CameraControlComponent)
	{
		CameraControlComponent->InitializeCamera(SpringArmComponent, CameraComponent);
	}
}

void ACBChaserCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ACBChaserCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// 입력 설정 데이터 에셋이 유효한지 확인 (nullptr일 경우 에러 발생)
	checkf(InputConfig, TEXT("%s 의 Input Config 유효하지 않음."), *GetName());

	// 현재 컨트롤러에서 로컬 플레이어 객체를 가져옴
	ULocalPlayer* LocalPlayer = GetController<APlayerController>()->GetLocalPlayer();

	// 로컬 플레이어의 서브시스템을 가져옴 (Enhanced Input 시스템 사용)
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

	// 서브시스템이 유효한지 확인
	check(Subsystem);

	// 입력 매핑 컨텍스트를 서브시스템에 추가 (우선순위 0)
	Subsystem->AddMappingContext(InputConfig->DefaultMappingContext, 0);

	// 입력 컴포넌트를 UCBInputComponent 타입으로 캐스팅 (실패 시 에디터에서 에러 발생)
	UCBInputComponent* CBInputComponent = CastChecked<UCBInputComponent>(PlayerInputComponent);

	// Input 태그에 해당하는 Input Action을 바인딩
	CBInputComponent->BindNativeInputAction(InputConfig, CBGameplayTags::Input_Action_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
	CBInputComponent->BindNativeInputAction(InputConfig, CBGameplayTags::Input_Action_Look, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
	CBInputComponent->BindNativeInputAction(InputConfig, CBGameplayTags::Input_Action_Camera_Zoom, ETriggerEvent::Triggered, this, &ThisClass::Input_Camera_Zoom);
	
	// InputConfig 의 Ability Input Actions 배열의 모든 액션을 바인딩 (Input_AbilityInputPressed, Input_AbilityInputReleased 함수와 연결)
	CBInputComponent->BindAbilityInputAction(InputConfig, this, &ThisClass::Input_AbilityInputPressed, &ThisClass::Input_AbilityInputReleased);
}

void ACBChaserCharacter::Input_Move(const FInputActionValue& InputActionValue)
{
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
	
	if (CameraControlComponent)
	{
		CameraControlComponent->Input_Look(LookAxisVector);
	}
}

void ACBChaserCharacter::Input_Camera_Zoom(const FInputActionValue& InputActionValue)
{
	const float WheelValue = InputActionValue.Get<float>();
	
	if (CameraControlComponent)
	{
		CameraControlComponent->Input_Camera_Zoom(WheelValue);
	}
}

void ACBChaserCharacter::Input_AbilityInputPressed(FGameplayTag InInputTag)
{
	CBAbilitySystemComponent->OnAbilityInputPressed(InInputTag);
}

void ACBChaserCharacter::Input_AbilityInputReleased(FGameplayTag InInputTag)
{
	CBAbilitySystemComponent->OnAbilityInputReleased(InInputTag);
}

UCBChaserCombatComponent* ACBChaserCharacter::GetChaserCombatComponent() const
{
	return Cast<UCBChaserCombatComponent>(CombatComponent);
}
