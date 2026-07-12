// project
#include "AnimInstances/CBCharacterAnimInstance.h"
#include "Characters/CBBaseCharacter.h"
#include "CBAbilitySystemLibrary.h"
#include "CBGameplayTags.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "DataAssets/Movement/CBCharacterMovementData.h"

// engine
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"
#include "AlphaBlend.h"

// 게임 스레드에서 실행되는 업데이트 함수 (UObject 접근 가능)
void UCBCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 움직임 데이터 가져오기 (캐싱)
	UpdateBasicMovementData();

	// 어빌리티/컴뱃 관련 데이터 가져오기 (캐싱)
	UpdateCombatAndAbilityData();
}

// 워커 스레드에서 실행되는 업데이트 함수 (UObject 접근 불가 (외부 접근 불가, 내부 데이터로만), 매우 빠름)
void UCBCharacterAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	if (!GetCachedCharacter(CachedCharacter) || !GetCachedCMC(CachedCMC))
	{
		return;
	}

	// 데이터 업데이트 (내부 데이터 복사)
	CurrentVelocity = CachedVelocity;
	CurrentVelocitySize = CurrentVelocity.Size2D();
	CurrentAccelerationSize = CachedAcceleration.Size2D();
	
	// 목표 개이트 최대 속도 대비 현재 속도 비율 (출발 판정 + 정지 애니메이션 블렌드용)
	CurrentSpeedRatio = (CachedGaitMaxSpeed > KINDA_SMALL_NUMBER) ? (CurrentVelocitySize / CachedGaitMaxSpeed) : 0.f;

	// 상태 업데이트
	bHasAcceleration = CurrentAccelerationSize > KINDA_SMALL_NUMBER;

	// 로컬 속도 계산 (캐릭터의 회전에 따라 월드 속도를 로컬 속도로 변환)
	// 월드 속도를 캐릭터 회전의 역으로 돌려 로컬 속도로 만듦.
	// LocalVelocity.X = 항상 캐릭터 앞뒤, LocalVelocity.Y = 항상 캐릭터 좌우
	FVector LocalVelocity = CachedActorRotation.UnrotateVector(CurrentVelocity);
	
	if (!LocalVelocity.IsNearlyZero())
	{
		// 정규화 (크기를 1로 만듦, 속도 크기는 버리고 방향만 남김)
		// 앞(1,0), 오른쪽 45도(0.707, 0.707)
		LocalVelocity.Normalize();
	}

	// 로컬 속도를 보간하여 InputX, InputY에 적용
	// 방향이 급변해도 블렌드 스페이스 샘플 지점이 부드럽게 이동(팝핑 방지)
	// Input [X] -> LocalVelocity [Y] (축 스왑)
	// Input [Y] -> LocalVelocity [X] (축 스왑)
	// [축 스왑] 블렌드 스페이스 규약이 X축은 캐릭터 좌우, Y축은 캐릭터 앞뒤에 매핑되어 있기 때문
	MoveX = FMath::FInterpTo(MoveX, LocalVelocity.Y, DeltaSeconds, 10.f);
	MoveY = FMath::FInterpTo(MoveY, LocalVelocity.X, DeltaSeconds, 10.f);
}

void UCBCharacterAnimInstance::UpdateBasicMovementData()
{
	if (GetCachedCharacter(CachedCharacter) && GetCachedCMC(CachedCMC))
	{
		CachedVelocity = CachedCharacter.Get()->GetVelocity();
		CachedAcceleration = CachedCMC.Get()->GetCurrentAcceleration();
		CachedActorRotation = CachedCharacter.Get()->GetActorRotation();
		bIsInAir = CachedCMC.Get()->IsFalling();
	}
}

void UCBCharacterAnimInstance::UpdateCombatAndAbilityData()
{
	if (!GetCachedCharacter(CachedCharacter)) return;

	// 개이트 태그를 한 번만 결정해 enum 판정과 최대 속도 조회에 함께 사용 (공용 헬퍼 — Sprint > Walk > 기본 Run 우선순위)
	const FGameplayTag GaitTag = UCBAbilitySystemLibrary::GetCurrentGaitTag(CachedCharacter.Get()->GetCBAbilitySystemComponent());

	// 태그 → enum 매핑
	if (GaitTag == CBGameplayTags::Movement_Sprint)
	{
		CurrentLocomotionGait = ECBLocomotionGait::Sprint;
	}
	else if (GaitTag == CBGameplayTags::Movement_Walk)
	{
		CurrentLocomotionGait = ECBLocomotionGait::Walk;
	}
	else
	{
		// 기본 상태
		CurrentLocomotionGait = ECBLocomotionGait::Run;
	}

	// 출발 판정용 목표 개이트 최대 속도 캐싱 (UObject 접근이라 게임 스레드에서만 수행)
	if (UCBCharacterMovementData* MovementData = CachedCharacter.Get()->GetMovementDataAsset())
	{
		CachedGaitMaxSpeed = MovementData->GetSpeedForTag(GaitTag);
	}
}

bool UCBCharacterAnimInstance::GetCachedCharacter(TWeakObjectPtr<ACBBaseCharacter>& OutCharacter)
{
	// 캐싱된 Character가 이미 존재하면 그대로 반환
	if (OutCharacter.IsValid())
	{
		return true;
	}

	// 유효하지 않다면 캐싱 시도
	OutCharacter = Cast<ACBBaseCharacter>(TryGetPawnOwner());

	return OutCharacter.IsValid();
}

bool UCBCharacterAnimInstance::GetCachedCMC(TWeakObjectPtr<UCharacterMovementComponent>& OutCMC)
{
	// 캐싱된 CMC가 이미 존재하면 그대로 반환
	if (OutCMC.IsValid())
	{
		return true;
	}

	// 유효하지 않다면 캐싱 시도
	if (GetCachedCharacter(CachedCharacter))
	{
		OutCMC = CachedCharacter.Get()->GetCharacterMovement();
	}

	return OutCMC.IsValid();
}

bool UCBCharacterAnimInstance::IsMoving() const
{
	return bHasAcceleration || CurrentVelocitySize > 10.f;
}

bool UCBCharacterAnimInstance::IsStopping() const
{
	// 가속이 없고, 현재 속도 비율이 정지 속도 비율 임계값보다 크면 "정지중"
	return !bHasAcceleration && CurrentSpeedRatio > StopSpeedRatioThreshold;
}

bool UCBCharacterAnimInstance::IsStarting() const
{
	// 가속 중이면서, 현재 속도 비율이 출발 속도 비율 임계값보다 작으면 "출발중"
	return bHasAcceleration && CurrentSpeedRatio < StartSpeedRatioThreshold;
}

void UCBCharacterAnimInstance::LockCurrentGait(const FAnimUpdateContext& UpdateContext, const FAnimNodeReference& NodeReference)
{
	// 스테이트 진입 시점의 개이트를 고정 (Start/Stop 재생 중 개이트가 바뀌어도 클립 유지)
	LockedLocomotionGait = CurrentLocomotionGait;
}

void UCBCharacterAnimInstance::PlayMontage(UAnimMontage* InMontage, float PlayRate)
{
	if (!InMontage) return;

	// 블렌드 인 시간을 재생 속도로 스케일(BlendIn ÷ PlayRate).
	// 블렌드 웨이트는 실제 시간(초) 기준이라, 재생 속도가 빠르면 같은 실시간 블렌드가 몽타주 구간을
	// 더 많이 잠식해 초반 포즈가 뭉개진다(공격 속도가 높을수록 스윙 초반 트레이스 누락 등 문제 발생).
	// 재생 속도로 나눠 몽타주 시간 축에 맞추면 배속과 무관하게 항상 같은 비율만 블렌드한다. (PlayRate=1이면 원래 값 유지)
	FAlphaBlendArgs BlendInArgs = InMontage->GetBlendInArgs();
	if (PlayRate > 0.f)
	{
		BlendInArgs.BlendTime /= PlayRate;
	}

	Montage_PlayWithBlendIn(InMontage, BlendInArgs, PlayRate);
}

void UCBCharacterAnimInstance::OnCombatTagChanged(const FGameplayTag InTag, int32 InCount)
{
	// 태그가 1개 이상 추가되면 true, 모두 제거되면 false
	bIsCombatMode = (InCount > 0);
}

void UCBCharacterAnimInstance::OnCharacterSystemReady()
{
	// 애니메이션 데이터 초기화 (ASC 준비 완료)
	InitAnimData();
}

// OnCharacterSystemReady 함수에서 호출되는 애니메이션 데이터 초기화 함수. ASC가 준비된 후에 실행되어야 하는 초기화 로직을 포함.
// OnCharacterSystemReady가 1회만 실행되도록 보장되므로 별도의 중복 방지 플래그는 두지 않는다.
void UCBCharacterAnimInstance::InitAnimData()
{
	// 소유 캐릭터 캐싱 보장 (베이스는 캐릭터를 캐싱하지 않으므로 여기서 확보)
	if (!GetCachedCharacter(CachedCharacter))
	{
		return;
	}

	// 델리게이트 설정
	if (UCBAbilitySystemComponent* ASC = CachedCharacter->GetCBAbilitySystemComponent())
	{
		// 전투 모드 태그 변경 시 OnCombatTagChanged 함수 호출
		ASC->RegisterGameplayTagEvent(
			CBGameplayTags::Status_Combat_InCombat,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UCBCharacterAnimInstance::OnCombatTagChanged);
	}
}
