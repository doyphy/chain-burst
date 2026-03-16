// project
#include "Components/Camera/CBCameraControlComponent.h"
#include "Characters/CBChaserCharacter.h"

// engine
#include "GameFramework/SpringArmComponent.h"

UCBCameraControlComponent::UCBCameraControlComponent()
{
	// Tick 활성화
	PrimaryComponentTick.bCanEverTick = true;
}

void UCBCameraControlComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedCharacter = Cast<ACBChaserCharacter>(GetOwner());
}

void UCBCameraControlComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!SpringArm) return;

	// 값이 다를 때만 연산 (최적화)
	if (!FMath::IsNearlyEqual(CurrentZoomAlpha, TargetZoomAlpha, 0.001f))
	{
		CurrentZoomAlpha = FMath::FInterpTo(CurrentZoomAlpha, TargetZoomAlpha, DeltaTime, ZoomInterpSpeed);
		
		float NewLength = FMath::Lerp(ZoomConfig.MinLength, ZoomConfig.MaxLength, CurrentZoomAlpha);

		FVector NewOffset = FMath::Lerp(ZoomConfig.MinSocketOffset, ZoomConfig.MaxSocketOffset, CurrentZoomAlpha);

		SpringArm->TargetArmLength = NewLength;
		SpringArm->SocketOffset = NewOffset;
	}
}

void UCBCameraControlComponent::InitializeCamera(USpringArmComponent* InSpringArmComponent, UCameraComponent* InCameraComponent)
{
	// 유효성 검사
	if (!ensureMsgf(InSpringArmComponent, TEXT("[%s] 스프링 암이 유효하지 않음."), *GetOwner()->GetName()))
	{
		return; 
	}
    
	if (!ensureMsgf(InCameraComponent, TEXT("[%s]카메라가 유효하지 않음."), *GetOwner()->GetName()))
	{
		return;
	}
	
	SpringArm = InSpringArmComponent;
	Camera = InCameraComponent;
	
	SpringArm->TargetOffset = FVector(0.f,0.f,50.f);

	// 기본 길이/오프셋 설정 (최대 줌 인/아웃의 30%로 초기화)
	CurrentZoomAlpha = 0.3;
	
	TargetZoomAlpha = CurrentZoomAlpha;
	DefaultTargetArmLength = FMath::Lerp(ZoomConfig.MinLength, ZoomConfig.MaxLength, CurrentZoomAlpha);
	DefaultSocketOffset = FMath::Lerp(ZoomConfig.MinSocketOffset, ZoomConfig.MaxSocketOffset, CurrentZoomAlpha);;
	
	SpringArm->SocketOffset = DefaultSocketOffset;
	SpringArm->TargetArmLength = DefaultTargetArmLength;
}

void UCBCameraControlComponent::Input_Look(const FVector2D& InLookAxisVector)
{
	if (!CachedCharacter) return;
	
	// 컨트롤러 회전
	CachedCharacter->AddControllerYawInput(InLookAxisVector.X);
	CachedCharacter->AddControllerPitchInput(InLookAxisVector.Y);
}

void UCBCameraControlComponent::Input_Camera_Zoom(const float& InWheelValue)
{
	if (FMath::IsNearlyZero(InWheelValue))
	{
		return;	
	}
	
	// InputVal: 휠 올림(+1.0), 휠 내림(-1.0)
	// 줌 인(Up, +1) -> 가까워져야 함 -> Alpha 0.0 쪽으로 이동 -> 빼기(-)
	// 줌 아웃(Down, -1) -> 멀어져야 함 -> Alpha 1.0 쪽으로 이동 -> 더하기(+)
	// 따라서 InputVal에 -1을 곱해서 방향을 뒤집어 줌
	const float ZoomDirection = -1.0f; 

	// 목표 Alpha값 갱신
	TargetZoomAlpha += (InWheelValue * ZoomStep * ZoomDirection);

	// 0.0 ~ 1.0 사이를 벗어나지 않게 제한
	TargetZoomAlpha = FMath::Clamp(TargetZoomAlpha, 0.0f, 1.0f);
}
