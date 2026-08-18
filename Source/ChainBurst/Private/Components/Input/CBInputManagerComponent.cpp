// project
#include "Components/Input/CBInputManagerComponent.h"
#include "Components/Input/CBInputComponent.h"
#include "DataAssets/Input/CBInputConfig.h"
#include "DataAssets/Movement/CBCharacterMovementData.h"
#include "Characters/CBChaserCharacter.h"
#include "Components/Camera/CBCameraControlComponent.h"
#include "Components/Movement/CBCharacterRotationComponent.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "CBAbilitySystemLibrary.h"
#include "CBGameplayTags.h"

// engine
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"

// 로드아웃에서 호출. 입력 설정을 주입하는 세터 (주입 후 셋업을 지연 시도)
void UCBInputManagerComponent::SetInputConfig(UCBInputConfig* InInputConfig)
{
	InputConfig = InInputConfig;

	// 입력 설정이 주입되었으니 나머지 전제조건이 갖춰졌으면 바인딩되도록 지연 시도
	TrySetupInput();
}

// SetupPlayerInputComponent에서 호출. 입력 컴포넌트를 받아 셋업을 지연 시도
void UCBInputManagerComponent::SetupPlayerInput(UInputComponent* InInputComponent)
{
	// 폰의 입력 컴포넌트를 커스텀 타입으로 캐스팅해 보관 (프로젝트 설정상 항상 UCBInputComponent)
	CachedInputComponent = CastChecked<UCBInputComponent>(InInputComponent);

	// 입력 컴포넌트가 준비된 지금, 나머지 전제조건(InputConfig)이 갖춰졌으면 바인딩되도록 지연 시도
	TrySetupInput();
}

// 게임플레이 입력(매핑 컨텍스트)을 허용하는 함수. 캐릭터를 조작하지 않는 레벨에서는 호출하지 않음.
void UCBInputManagerComponent::AllowGameplayInput()
{
	bGameplayInputAllowed = true;

	// 나머지 전제조건이 이미 갖춰져 있으면 여기서 바로 등록됨
	TryRegisterMappingContexts();
}

void UCBInputManagerComponent::TrySetupInput()
{
	// 준비된 것부터 처리
	TryBindInputActions();
	TryRegisterMappingContexts();
}

void UCBInputManagerComponent::TryBindInputActions()
{
	// 이미 바인딩이 완료되었으면 종료 (중복 방지)
	if (bInputBindingsSetup) return;

	// 입력 설정(로드아웃 주입)과 입력 컴포넌트(SetupPlayerInputComponent)가 모두 준비되어야 함
	if (!InputConfig || !CachedInputComponent) return;

	if (!GetOwningController<APlayerController>()) return;

	// 입력 액션 바인딩 수행
	BindInputActions();
	bInputBindingsSetup = true;
}

void UCBInputManagerComponent::TryRegisterMappingContexts()
{
	// 이미 등록되었으면 종료 (중복 방지)
	if (bMappingContextsRegistered) return;

	// 허용 전에는 매핑 컨텍스트를 붙이지 않음.
	// 로비처럼 조작하지 않는 레벨에서는 IMC 가 아예 없으므로, UI 가 걷어내주지 않아도 조작이 성립하지 않음.
	if (!bGameplayInputAllowed) return;

	// 등록 대상은 로컬 플레이어 서브시스템이라 InputComponent 는 필요 없음 (바인딩과 다른 지점)
	if (!InputConfig) return;

	// 매핑 컨텍스트 등록을 위해 로컬 플레이어 컨트롤러가 필요
	if (!GetOwningController<APlayerController>()) return;

	// IMC 등록 수행
	RegisterMappingContexts();
	bMappingContextsRegistered = true;
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
	// 입력 잠금 여부 확인 (시스템 잠금 + 피벗 잠금)
	if (bIsInputLocked || bPivotInputLocked) return;

	ACBChaserCharacter* Character = GetOwningChaser();
	if (!Character) return;

	// 입력된 이동 벡터 가져오기
	const FVector2D MovementVector = InputActionValue.Get<FVector2D>();

	// 입력 축 규약(IMC의 Swizzle 매핑 기준): Y = 전/후(W/S), X = 좌/우(A/D).
	// 아래에서 월드 전방 벡터에는 ForwardInput, 우측 벡터에는 RightInput을 곱함.
	const float ForwardInput = MovementVector.Y;
	const float RightInput = MovementVector.X;

	// 카메라 기준 이동 방향(플레이어 의도)을 계산해 피벗 감지와 회전 컴포넌트에 사용.
	// 회전에서는 Sprint가 이동 방향으로 몸을 돌리는데 사용, Walk/Run은 무시(카메라 방향을 봄).
	if (AController* Controller = Character->GetController())
	{
		// 카메라 회전 Yaw 값 (좌우 회전각) 가져오기
		const FRotator CameraYaw(0, Controller->GetControlRotation().Yaw, 0);

		// 카메라의 전방/우측 방향 벡터 가져오기
		const FVector CameraForward = FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::X);
		const FVector CameraRight = FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::Y);

		// 카메라의 전방 방향 벡터와 우측 방향 벡터에 입력 값을 곱한 후 더함 (월드 공간에서의 이동 방향)
		const FVector DesiredDir = CameraForward * ForwardInput + CameraRight * RightInput;

		// 피벗 감지 — 입력 방향이 현재 속도 방향과 크게 어긋나면 이동 입력을 잠그고 이번 입력은 무시.
		// (잠금 동안 자연 감속 → Stop 재생 → 해제 후 유지 중인 입력으로 Start 재출발)
		if (TryDetectPivot(DesiredDir))
		{
			return;
		}

		// 이동 방향을 회전 컴포넌트에 넘기기
		if (UCBCharacterRotationComponent* RotationComp = Character->GetCharacterRotationComponent())
		{
			RotationComp->SetMoveInputDirection(DesiredDir);
		}
	}

	// 실제 이동 적용은 캐릭터의 현재 facing yaw 기준.
	// 액터 회전은 UCBCharacterRotationComponent가 개이트별 회전 속도로 보간한 값이므로,
	// 속도 방향이 회전을 따라 지연됨 — Walk/Run은 코너 관성, Sprint는 몸이 향한 정면으로 정렬.
	const FRotator YawRotation(0, Character->GetActorRotation().Yaw, 0);

	// Sprint 여부 판별 (orient-to-movement 이동 적용 분기)
	const bool bIsSprinting =
		UCBAbilitySystemLibrary::GetCurrentGaitTag(Character->GetCBAbilitySystemComponent()) == CBGameplayTags::Status_Movement_Gait_Sprint;

	if (bIsSprinting)
	{
		// Sprint: 방향은 회전(몸이 입력 방향을 향함)이 전담하므로, 이동은 facing 정면으로 입력 크기만큼 직진만 한다.
		// (전/후·좌/우 분해를 하면 이미 회전된 몸에 방향이 이중 적용되어 엉뚱한 방향으로 감)
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		Character->AddMovementInput(ForwardDirection, FMath::Min(1.0f, MovementVector.Size()));
	}
	else
	{
		// Walk/Run: facing 기준 전/후·좌/우 분해 (aim-facing 스트레이핑)
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		Character->AddMovementInput(ForwardDirection, ForwardInput);
		Character->AddMovementInput(RightDirection, RightInput);
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

bool UCBInputManagerComponent::TryDetectPivot(const FVector& InDesiredDir)
{
	ACBChaserCharacter* Character = GetOwningChaser();
	if (!Character) return false;

	// 공중에서는 피벗 없음 (피벗은 지상 급반전 개념 — 공중 방향 전환에 입력 잠금이 걸리면 오동작)
	const UCharacterMovementComponent* CMC = Character->GetCharacterMovement();
	if (!CMC || CMC->IsFalling()) return false;

	// 개이트별 피벗 파라미터 조회 — 이동 데이터가 없으면 피벗 비활성
	UCBCharacterMovementData* MovementData = Character->GetMovementDataAsset();
	if (!MovementData) return false;

	const FGameplayTag GaitTag = UCBAbilitySystemLibrary::GetCurrentGaitTag(Character->GetCBAbilitySystemComponent());
	const FCBGaitMovementData* GaitData = MovementData->FindGaitData(GaitTag);
	if (!GaitData) return false;

	// 속도 게이트 — 개이트 최대 속도 대비 일정 비율 이상으로 이동 중이어야 피벗 (저속 방향 전환은 잠금 없이 그냥 방향 전환)
	const FVector Velocity2D = FVector(Character->GetVelocity().X, Character->GetVelocity().Y, 0.f);
	if (Velocity2D.Size() < GaitData->MaxSpeed * PivotMinSpeedRatio) return false;

	// 입력 방향이 사실상 없으면 (데드존) 판정 불가
	const FVector DesiredDir2D = FVector(InDesiredDir.X, InDesiredDir.Y, 0.f);
	if (DesiredDir2D.IsNearlyZero()) return false;

	// 입력 방향(의도)과 현재 속도 방향의 각도 차이 계산
	// 내적으로 각도 계산 (코사인) = -1 ~ 1 (180도 ~ 0도)
	const float CosAngle = FVector::DotProduct(Velocity2D.GetSafeNormal(), DesiredDir2D.GetSafeNormal());
	// 아크코사인으로 각도(도 단위)로 변환
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosAngle, -1.f, 1.f)));

	// 임계값 미만이면 피벗 아님
	if (AngleDegrees < GaitData->PivotAngleThreshold) return false;

	// 피벗 감지 — 이동 입력을 개이트별 시간만큼 잠금 (해제는 UnlockPivotInput 타이머)
	bPivotInputLocked = true;
	GetWorld()->GetTimerManager().SetTimer(
		PivotUnlockTimerHandle, this, &ThisClass::UnlockPivotInput, GaitData->PivotInputLockDuration, false);

	return true;
}

// 피벗 이동 입력 잠금 해제 함수 (타이머 콜백)
void UCBInputManagerComponent::UnlockPivotInput()
{
	// 잠금 해제 — 플레이어가 입력을 유지 중이면 다음 Input_Move부터 새 방향으로 재출발
	bPivotInputLocked = false;
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
