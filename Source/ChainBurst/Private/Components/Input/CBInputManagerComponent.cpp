// project
#include "Components/Input/CBInputManagerComponent.h"
#include "Components/Input/CBInputComponent.h"
#include "DataAssets/Input/CBInputConfig.h"
#include "Characters/CBChaserCharacter.h"
#include "Components/Camera/CBCameraControlComponent.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "CBGameplayTags.h"

// engine
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

void UCBInputManagerComponent::SetInputConfig(UCBInputConfig* InInputConfig)
{
	InputConfig = InInputConfig;

	// 입력 설정이 주입되었으니 나머지 전제조건이 갖춰졌으면 바인딩되도록 지연 시도
	TrySetupInput();
}

void UCBInputManagerComponent::SetupPlayerInput(UInputComponent* InInputComponent)
{
	// 폰의 입력 컴포넌트를 커스텀 타입으로 캐스팅해 보관 (프로젝트 설정상 항상 UCBInputComponent)
	CachedInputComponent = CastChecked<UCBInputComponent>(InInputComponent);

	// 입력 컴포넌트가 준비된 지금, 나머지 전제조건(InputConfig)이 갖춰졌으면 바인딩되도록 지연 시도
	TrySetupInput();
}

void UCBInputManagerComponent::TrySetupInput()
{
	// 이미 바인딩이 완료되었으면 종료 (중복 방지)
	if (bInputBindingsSetup) return;

	// 입력 설정(로드아웃 주입)과 입력 컴포넌트(SetupPlayerInputComponent)가 모두 준비되어야 함
	if (!InputConfig || !CachedInputComponent) return;

	// 매핑 컨텍스트 등록을 위해 로컬 플레이어 컨트롤러가 필요
	if (!GetOwningController<APlayerController>()) return;

	// 전제조건 충족 → 실제 바인딩 수행
	SetupInputBindings();
	bInputBindingsSetup = true;
}

void UCBInputManagerComponent::SetupInputBindings()
{
	// 매핑 컨텍스트 등록 → 액션 바인딩 순서로 수행
	RegisterMappingContexts();
	BindInputActions();
}

void UCBInputManagerComponent::RegisterMappingContexts()
{
	// 현재 컨트롤러에서 로컬 플레이어 객체를 가져옴
	ULocalPlayer* LocalPlayer = GetOwningController<APlayerController>()->GetLocalPlayer();

	// 로컬 플레이어의 서브시스템을 가져옴 (Enhanced Input 시스템 사용)
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

	// 서브시스템이 유효한지 확인
	check(Subsystem);

	// 캐릭터가 사용하는 매핑 컨텍스트를 데이터에 지정된 우선순위대로 서브시스템에 추가
	for (const FCBMappingContextEntry& Entry : InputConfig->MappingContexts)
	{
		if (Entry.MappingContext)
		{
			Subsystem->AddMappingContext(Entry.MappingContext, Entry.Priority);
		}
	}
}

void UCBInputManagerComponent::BindInputActions()
{
	// Input 태그에 해당하는 Input Action을 바인딩 (트리거 이벤트는 InputConfig 데이터에서 결정)
	CachedInputComponent->BindNativeInputAction(InputConfig, CBGameplayTags::Input_Action_Move, this, &ThisClass::Input_Move);
	CachedInputComponent->BindNativeInputAction(InputConfig, CBGameplayTags::Input_Action_Look, this, &ThisClass::Input_Look);
	CachedInputComponent->BindNativeInputAction(InputConfig, CBGameplayTags::Input_Action_Camera_Zoom, this, &ThisClass::Input_Camera_Zoom);

	// InputConfig 의 Ability Input Actions 배열의 모든 액션을 바인딩 (Input_AbilityInputPressed, Input_AbilityInputReleased 함수와 연결)
	CachedInputComponent->BindAbilityInputAction(InputConfig, this, &ThisClass::Input_AbilityInputPressed, &ThisClass::Input_AbilityInputReleased);
}

ACBChaserCharacter* UCBInputManagerComponent::GetOwningChaser()
{
	// 이미 캐싱된 소유 캐릭터가 있으면 그대로 반환, 없으면 캐싱 시도
	if (!CachedChaser)
	{
		CachedChaser = GetOwningPawn<ACBChaserCharacter>();
	}
	return CachedChaser;
}

void UCBInputManagerComponent::Input_Move(const FInputActionValue& InputActionValue)
{
	// 입력 잠금 여부 확인
	if (bIsInputLocked) return;

	ACBChaserCharacter* Character = GetOwningChaser();
	const FVector2D MovementVector = InputActionValue.Get<FVector2D>();

	if (AController* Controller = Character->GetController())
	{
		// Yaw 회전값을 기준으로 전방 및 우측 방향 벡터 계산
		const FRotator YawRotation(0, Controller->GetControlRotation().Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// 입력된 이동 벡터를 기반으로 캐릭터 이동
		Character->AddMovementInput(ForwardDirection, MovementVector.Y);
		Character->AddMovementInput(RightDirection, MovementVector.X);
	}
}

void UCBInputManagerComponent::Input_Look(const FInputActionValue& InputActionValue)
{
	const FVector2D LookAxisVector = InputActionValue.Get<FVector2D>();

	if (UCBCameraControlComponent* CameraControl = GetOwningChaser()->GetCameraControlComponent())
	{
		CameraControl->Input_Look(LookAxisVector);
	}
}

void UCBInputManagerComponent::Input_Camera_Zoom(const FInputActionValue& InputActionValue)
{
	const float WheelValue = InputActionValue.Get<float>();

	if (UCBCameraControlComponent* CameraControl = GetOwningChaser()->GetCameraControlComponent())
	{
		CameraControl->Input_Camera_Zoom(WheelValue);
	}
}

void UCBInputManagerComponent::Input_AbilityInputPressed(FGameplayTag InInputTag)
{
	if (UCBAbilitySystemComponent* ASC = GetOwningChaser()->GetCBAbilitySystemComponent())
	{
		ASC->OnAbilityInputPressed(InInputTag);
	}
}

void UCBInputManagerComponent::Input_AbilityInputReleased(FGameplayTag InInputTag)
{
	if (UCBAbilitySystemComponent* ASC = GetOwningChaser()->GetCBAbilitySystemComponent())
	{
		ASC->OnAbilityInputReleased(InInputTag);
	}
}
