// project
#include "AbilitySystem/Abilities/Movement/CBGADash.h"
#include "CBAbilitySystemLibrary.h"
#include "CBGameplayTags.h"
#include "Characters/CBChaserCharacter.h"
#include "Components/Input/CBInputManagerComponent.h"

// engine
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"

UCBGADash::UCBGADash()
{
	// 로컬 입력으로 발동되어 즉시 반응해야 하므로 예측 실행
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 어빌리티 식별 태그 (외부에서 이 어빌리티를 태그로 식별·취소·차단할 때 사용)
	SetAssetTags(FGameplayTagContainer(CBGameplayTags::Ability_Movement_Dash));

	// 어빌리티 활성 동안 "대시 중" 상태 태그 자동 부여/제거
	// (CBLocomotionProcessor의 대시 전용 감속 판별 + GA_Sprint의 ActivationRequiredTags 활성화 조건에 사용)
	ActivationOwnedTags.AddTag(CBGameplayTags::Status_Movement_Dashing);

	// 대시 성공 시 함께 활성화할 Sprint 어빌리티 태그 기본값
	SprintAbilityTag = CBGameplayTags::Ability_Movement_Sprint;
}

void UCBGADash::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 몽타주 재생 가능한지 확인 (베이스가 액션 컴포넌트/액션 태그 검사 후 세팅)
	if (bCanPlayMontage == false)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 커밋 — 쿨다운 GE 적용 (쿨다운 중 재활성화는 CanActivateAbility가 이미 차단)
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// [로컬] 방향은 카메라 기준 입력이라 소유 클라(호스트 포함)만 정할 수 있음
	if (ActorInfo->IsLocallyControlled())
	{
		// 현재 카메라 기준 입력 방향 가져오기
		const FVector Direction = Local_CalculateDashDirection();

		// 서버로 방향을 보냄 (호스트는 서버 자신이라 불필요)
		if (!HasAuthority(&ActivationInfo))
		{
			Local_SendDashDirectionToServer(Direction);
		}

		// 해당 방향으로 대시 발동
		StartDash(Direction);
		return;
	}

	// [서버] 원격 클라가 보낸 방향을 받은 뒤 재생.
	// 방향은 활성화 RPC 바로 뒤에 같은 ASC 채널(Reliable)로 오므로 보통 같은 패킷에 실려 옴.
	// 이미 도착해 있으면 GAS 캐시에서 즉시 콜백됨 (CallReplicatedTargetDataDelegatesIfSet)
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FPredictionKey ActivationKey = ActivationInfo.GetActivationPredictionKey();
		ASC->AbilityTargetDataSetDelegate(Handle, ActivationKey).AddUObject(this, &ThisClass::Auth_HandleDashDirectionReceived);
		ASC->CallReplicatedTargetDataDelegatesIfSet(Handle, ActivationKey);
	}
}

void UCBGADash::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// [서버] 방향을 받기 전에 끝난 경우 대비 - 수신 구독 해제
	if (ActorInfo && !ActorInfo->IsLocallyControlled())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).RemoveAll(this);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FVector UCBGADash::Local_CalculateDashDirection() const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();

	// 카메라 기준 이동 입력 (수평 방향만)
	FVector Direction = FVector::ZeroVector;
	if (const ACBChaserCharacter* Chaser = Cast<ACBChaserCharacter>(Avatar))
	{
		if (const UCBInputManagerComponent* InputManager = Chaser->GetInputManagerComponent())
		{
			Direction = InputManager->Local_GetCameraRelativeMoveInput().GetSafeNormal2D();
		}
	}

	// 입력이 없으면 액터 정면
	if (Direction.IsNearlyZero() && Avatar)
	{
		Direction = Avatar->GetActorForwardVector();
	}

	return Direction;
}

void UCBGADash::Local_SendDashDirectionToServer(const FVector& InDirection)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	// 엔진 기본 타겟 데이터에 방향만 실음 (목표 트랜스폼의 회전 = 대시 방향).
	// 위치는 보내지 않음 - 워프 목표 위치는 서버가 자기 액터 위치로 계산함 (클라 좌표를 믿지 않음)
	FGameplayAbilityTargetData_LocationInfo* DirectionData = new FGameplayAbilityTargetData_LocationInfo();
	DirectionData->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	DirectionData->TargetLocation.LiteralTransform = FTransform(InDirection.Rotation());

	// 핸들이 소유권을 가짐 (TSharedPtr)
	const FGameplayAbilityTargetDataHandle DataHandle(DirectionData);

	// ActivateAbility 안이라 ScopedPredictionKey = 활성화 예측 키.
	// 서버는 이 키의 예측 창 안에서 몽타주 큐를 실행하므로, 이미 예측 재생한 소유 클라에는 다시 재생되지 않음
	ASC->ServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(),
		DataHandle, FGameplayTag(), ASC->ScopedPredictionKey);
}

void UCBGADash::Auth_HandleDashDirectionReceived(const FGameplayAbilityTargetDataHandle& InDataHandle, FGameplayTag /*InApplicationTag*/)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	// 방향 추출.
	FVector Direction = FVector::ZeroVector;
	if (const FGameplayAbilityTargetData* Data = InDataHandle.Get(0))
	{
		if (Data->HasEndPoint())
		{
			Direction = Data->GetEndPointTransform().GetRotation().GetForwardVector();
		}
	}

	// 한 번만 받으면 되므로 구독 해제 + 캐시 소비
	const FPredictionKey ActivationKey = CurrentActivationInfo.GetActivationPredictionKey();
	ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, ActivationKey).RemoveAll(this);
	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, ActivationKey);

	// 수평 방향만 사용. 비정상 값이면 액터 정면
	Direction = Direction.GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = GetAvatarActorFromActorInfo()->GetActorForwardVector();
	}

	// 수신 콜백은 클라가 보낸 예측 키의 예측 창 안에서 호출되므로, 여기서 실행하는 몽타주 큐도 그 키를 달고 나감
	StartDash(Direction);
}

void UCBGADash::StartDash(const FVector& InDirection)
{
	// 워프 타겟 계산에 쓸 방향 확정 (BuildActionCueParameters)
	DashDirection = InDirection;

	// 몽타주 재생 (인덱스는 SelectActionMontageIndex가 전투 상태로 분기 — 비전투 0 / 전투 1)
	PlayActionMontage();

	// 대시 성공 → Sprint 어빌리티 활성화
	// PlayActionMontage 내부 실패로 이미 종료됐으면 스킵. (IsActive())
	// 로컬 컨트롤에서만 시도 - Sprint 어빌리티가 로컬과 서버 모두 활성화되므로 서버 인스턴스가 중복 시도할 필요 없음.
	if (IsActive() && CurrentActorInfo->IsLocallyControlled() && SprintAbilityTag.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			// 전력 질주 어빌리티 활성화 (대시 이후 전력질주 유지)
			ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(SprintAbilityTag));
		}
	}
}

int32 UCBGADash::SelectActionMontageIndex()
{
	// 전투 상태(Status.Combat.InCombat)면 전투 대시(인덱스 1), 비전투면 일반 대시(인덱스 0)
	// 태그가 전 클라 복제(TagAndCountToAll)되므로 예측 클라/서버가 같은 인덱스를 계산
	return UCBAbilitySystemLibrary::IsCombatMode(GetAvatarActorFromActorInfo()) ? 1 : 0;
}

// 몽타주 재생 큐 파라미터에 모션 워핑 타겟을 실음
void UCBGADash::BuildActionCueParameters(FGameplayCueParameters& CueParams)
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar) return;

	// 모션 워핑 타겟(위치+방향). 방향은 StartDash가 확정한 값, 위치는 이 머신의 액터 위치 기준
	CueParams.Location = Avatar->GetActorLocation() + DashDirection * DashDistance;
	CueParams.Normal = DashDirection;
}
