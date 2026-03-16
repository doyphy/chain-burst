// project
#include "Components/Movement/CBCharacterRotationComponent.h"
#include "Characters/CBChaserCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

// engine
#include "Net/UnrealNetwork.h"

UCBCharacterRotationComponent::UCBCharacterRotationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UCBCharacterRotationComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedCharacter = Cast<ACBChaserCharacter>(GetOwner());
	
	if (CachedCharacter)
	{
		// 데이터 초기화
		CachedMovementComp = CachedCharacter->GetCharacterMovement();
		TargetRotation = CachedCharacter->GetActorRotation();
		SmoothedTargetRotation = TargetRotation;
	}
}

void UCBCharacterRotationComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CachedCharacter || !CachedMovementComp) return;

	// 로컬용 회전 처리
	if (CachedCharacter->IsLocallyControlled())
	{
		bool bIsMoving = CachedMovementComp->GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER;

		// 이동 중이면 카메라 방향을 기준으로 회전
		if (bIsMoving)
		{
			// 컨트롤러 회전 값(Yaw) 가져오기
			FRotator NewTargetRotation = FRotator(0.0f, CachedCharacter->GetControlRotation().Yaw, 0.0f);
    
			// 컨트롤러 회전 값과 TargetRotation 이 같지 않다면 TargetRotation 업데이트 및 서버에 전송
			if (!NewTargetRotation.Equals(TargetRotation, 0.1f))
			{
				TargetRotation = NewTargetRotation;
				Server_SetTargetRotation(TargetRotation);
			}
		}
	}

	// SmoothedTargetRotation 업데이트
	UpdateSmoothedTargetRotation(DeltaTime);
	
	// SmoothedTargetRotation 으로 보간
	FRotator CurrentRotation = CachedCharacter->GetActorRotation();
	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, SmoothedTargetRotation, DeltaTime, RotationInterpSpeed);
	CachedCharacter->SetActorRotation(NewRotation);
}

void UCBCharacterRotationComponent::UpdateSmoothedTargetRotation(float DeltaTime)
{
	// SmoothedTargetRotation 과 TargetRotation 의 차이 계산 
	float CurrentYaw = SmoothedTargetRotation.Yaw;
	float DesiredYaw = TargetRotation.Yaw;
	float YawDelta = FMath::FindDeltaAngleDegrees(CurrentYaw, DesiredYaw);

	// SmoothedTargetRotation 업데이트
	SmoothedTargetRotation = FRotator(0.0f, CurrentYaw + YawDelta, 0.0f);
}

// 리플리케이션 사용 시 ( UPROPERTY(Replicated) ) 필수로 구현해야 하는 함수.
// 매크로 덕분에 자동으로 선언해 클래스에 추가되어 있음.
// 리플리케이션 규칙을 정의하는 함수
void UCBCharacterRotationComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// TargetRotation 리플리케이션 설정
	// 본인한테는 복제 불필요 (본인은 이미 알고 있음)
	DOREPLIFETIME_CONDITION(UCBCharacterRotationComponent, TargetRotation, COND_SkipOwner);
}

// OnRep 구현 — 다른 클라이언트에서 TargetRotation 수신 시 호출
void UCBCharacterRotationComponent::OnRep_TargetRotation()
{
	// SmoothedTargetRotation의 목표를 갱신
	// Tick에서 RInterpTo로 자연스럽게 따라감
	SmoothedTargetRotation = TargetRotation;
}

void UCBCharacterRotationComponent::Server_SetTargetRotation_Implementation(FRotator NewTargetRotation)
{
	TargetRotation = NewTargetRotation;
}
