// project
#include "Components/Movement/CBCharacterRotationComponent.h"
#include "Characters/CBChaserCharacter.h"
#include "DataAssets/Movement/CBCharacterMovementData.h"
#include "CBAbilitySystemLibrary.h"
#include "CBGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"

// engine
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"

UCBCharacterRotationComponent::UCBCharacterRotationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	
	// 처음에는 Tick 비활성화 (OnCharacterSystemReady 함수에서 활성화)
	PrimaryComponentTick.bStartWithTickEnabled = false;
	
	SetIsReplicatedByDefault(true);
}

void UCBCharacterRotationComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetCachedCharacter(CachedCharacter) || !GetCachedCMC(CachedMovementComp)) return;

	// 현재 개이트 (회전 타겟 선택 + 회전 보간 속도에 공통 사용)
	const FGameplayTag GaitTag = GetCurrentGaitTag();

	// 로컬용 회전 처리
	if (CachedCharacter->IsLocallyControlled())
	{
		bool bIsMoving = CachedMovementComp->GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER;

		if (bIsMoving)
		{
			FRotator NewTargetRotation;

			// Sprint: 이동(입력) 방향으로 몸을 돌린다 (orient-to-movement) — 전방 클립 하나로 8방향 커버
			if (GaitTag == CBGameplayTags::Movement_Sprint && !CachedMoveInputDir.IsNearlyZero())
			{
				NewTargetRotation = FRotator(0.0f, CachedMoveInputDir.Rotation().Yaw, 0.0f);
			}
			// Walk/Run: 카메라 방향을 바라본다 (aim-facing, 3인칭 슈터 — 카메라 방향이 전방 공격 방향)
			else
			{
				NewTargetRotation = FRotator(0.0f, CachedCharacter->GetControlRotation().Yaw, 0.0f);
			}

			// 타겟이 바뀌었으면 갱신 및 서버 전송
			if (!NewTargetRotation.Equals(TargetRotation, 0.1f))
			{
				TargetRotation = NewTargetRotation;
				Server_SetTargetRotation(TargetRotation);
			}
		}
	}

	// SmoothedTargetRotation 업데이트
	UpdateSmoothedTargetRotation(DeltaTime);

	// SmoothedTargetRotation 으로 보간 (개이트별 회전 보간 속도 적용)
	FRotator CurrentRotation = CachedCharacter->GetActorRotation();
	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, SmoothedTargetRotation, DeltaTime, ResolveRotationInterpSpeed(GaitTag));
	CachedCharacter->SetActorRotation(NewRotation);
}

FGameplayTag UCBCharacterRotationComponent::GetCurrentGaitTag() const
{
	// 기본 개이트
	if (!CachedCharacter) return CBGameplayTags::Movement_Run;

	// 공용 헬퍼로 판별 (Sprint > Walk > 기본 Run 우선순위)
	return UCBAbilitySystemLibrary::GetCurrentGaitTag(CachedCharacter->GetAbilitySystemComponent());
}

float UCBCharacterRotationComponent::ResolveRotationInterpSpeed(FGameplayTag GaitTag) const
{
	// 개이트 데이터가 없을 때의 폴백
	float Result = RotationInterpSpeed;

	if (!CachedCharacter) return Result;

	UCBCharacterMovementData* MovementData = CachedCharacter->GetMovementDataAsset();
	if (!MovementData) return Result;

	// 데이터 에셋에서 조회 — 유효한 값이면 사용, 아니면 폴백 유지
	const float FoundSpeed = MovementData->GetRotationInterpSpeedForTag(GaitTag);
	if (FoundSpeed > 0.0f)
	{
		Result = FoundSpeed;
	}

	return Result;
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

void UCBCharacterRotationComponent::OnCharacterSystemReady()
{
	// Tick 활성화
	SetComponentTickEnabled(true);
}

bool UCBCharacterRotationComponent::GetCachedCharacter(TObjectPtr<ACBChaserCharacter>& OutCharacter)
{
	// 캐싱된 Character가 이미 존재하면 그대로 반환
	if (OutCharacter)
	{
		return true;
	}

	// 유효하지 않다면 캐싱 시도
	OutCharacter = GetOwningPawn<ACBChaserCharacter>();

	if (OutCharacter)
	{
		// 회전 데이터 초기화
		TargetRotation = CachedCharacter->GetActorRotation(); // 현재 회전을 타겟으로 설정
		SmoothedTargetRotation = TargetRotation; // 스무스 타겟도 동일하게 초기화
	}
	
	return (OutCharacter != nullptr);
}

bool UCBCharacterRotationComponent::GetCachedCMC(TObjectPtr<UCharacterMovementComponent>& OutCMC)
{
	// 캐싱된 CMC가 이미 존재하면 그대로 반환
	if (OutCMC)
	{
		return true;
	}

	// 유효하지 않다면 캐싱 시도
	if (ACBBaseCharacter* OwnerCharacter = GetOwningPawn<ACBBaseCharacter>())
	{
		OutCMC = OwnerCharacter->GetCharacterMovement();
	}
	
	return (OutCMC != nullptr);
}

void UCBCharacterRotationComponent::Server_SetTargetRotation_Implementation(FRotator NewTargetRotation)
{
	TargetRotation = NewTargetRotation;
}
