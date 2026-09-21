// project
#include "AbilitySystem/Abilities/Combat/CBHitReactAbility.h"
#include "CBAbilitySystemLibrary.h"
#include "CBGameplayTags.h"

// engine
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"

UCBHitReactAbility::UCBHitReactAbility()
{
	// 서버 권위 데미지로 발동되는 반응이므로 서버 주도 실행 (엣지 클래스에서 명시)
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// 재생할 피격 몽타주 태그
	BoundActionTag = CBGameplayTags::Action_Combat_HitReact;

	// 피격 시 진행 중인 공격 어빌리티 캔슬.
	CancelAbilityTag = CBGameplayTags::Ability_Combat_Attack;

	// 피격 상태 태그 부여
	ActivationOwnedTags.AddTag(CBGameplayTags::Status_Combat_Staggered);

	// 경직 중 또 맞으면 진행 중인 반응을 끝내고 다시 발동.
	// 엔진이 EndAbility(bWasCancelled = false)로 끝내므로 캔슬이 아닌 정상 종료로 처리됨.
	bRetriggerInstancedAbility = true;

	// 슈퍼아머(스킬 시전 등) 중에는 피격 반응 무시.
	// 데미지 GE는 그대로 적용되고 반응 모션만 생략.
	ActivationBlockedTags.AddTag(CBGameplayTags::Status_Combat_SuperArmor);

	// Event.Combat.HitReact 이벤트로 자동 발동되도록 트리거 등록
	RegisterEventTrigger(CBGameplayTags::Event_Combat_HitReact);
}

// 발동 가능 여부. 연쇄 경직 차단 구간이면 거부함.
bool UCBHitReactAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags /* = nullptr */,
	const FGameplayTagContainer* TargetTags /* = nullptr */, FGameplayTagContainer* OptionalRelevantTags /* = nullptr */) const
{
	// 공통 검사(사망 게이트·쿨다운·차단 태그 등)를 먼저 통과해야 함
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 연쇄 경직이 한도를 넘은 구간이면 반응하지 않음. (데미지는 그대로 들어감)
	const UWorld* World = GetWorld();
	if (World && IsChainStaggerBlocked(World->GetTimeSeconds()))
	{
		return false;
	}

	return true;
}

// 몽타주 재생(Super) 전에 연쇄 상태를 먼저 갱신.
void UCBHitReactAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (const UWorld* World = GetWorld())
	{
		UpdateChainStagger(World->GetTimeSeconds());
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 몽타주 재생에 실패해 Super 안에서 이미 끝났다면 넉백하지 않음.
	if (IsActive())
	{
		// 넉백 적용.
		ApplyKnockback();
	}
}

// 경직이 끝난 시각 기록. 재발동으로 인한 종료도 여기를 지나지만 같은 프레임이라 연쇄가 끊기지 않음.
void UCBHitReactAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const UWorld* World = GetWorld())
	{
		LastStaggerEndTime = World->GetTimeSeconds();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

int32 UCBHitReactAbility::SelectActionMontageIndex()
{
	// 전투 상태(Status.Combat.InCombat)면 전투 피격(인덱스 1), 비전투면 일반 피격(인덱스 0)
	// 태그가 전 클라 복제(TagAndCountToAll)되므로 예측 클라/서버가 같은 인덱스를 계산한다
	return UCBAbilitySystemLibrary::IsCombatMode(GetAvatarActorFromActorInfo()) ? 1 : 0;
}

#pragma region ChainStagger
// 차단 구간 판정. 연쇄 경과가 [MaxChainStaggerTime, MaxChainStaggerTime + ChainStaggerCooldown) 이면 차단.
// 차단 종료 시각을 따로 들지 않고 연쇄 시작 시각 하나에서 유도하므로, 이 함수는 상태를 바꾸지 않음.
// (CanActivateAbility 가 const 라 여기서 상태를 쓰려면 mutable 이 필요함).
bool UCBHitReactAbility::IsChainStaggerBlocked(float InNow) const
{
	// 제한이 꺼져 있거나 진행 중인 연쇄가 없으면 차단하지 않음
	if (MaxChainStaggerTime <= 0.f || ChainStartTime < 0.f)
	{
		return false;
	}

	// 현재 시간과 연쇄가 시작된 시간의 차이
	const float Elapsed = InNow - ChainStartTime;
	
	// 연쇄가 시작된 시간이 최대 연쇄 시간을 넘었음 && 최대 연쇄 시간 + 차단 시간을 넘지 않았음.
	// 연쇄 경직 차단시간이면 True 그 외에는 False
	return Elapsed >= MaxChainStaggerTime && Elapsed < MaxChainStaggerTime + ChainStaggerCooldown;
}

// 이번 피격이 같은 연쇄인지 판정. 아니면 연쇄를 새로 시작함.
void UCBHitReactAbility::UpdateChainStagger(float InNow)
{
	// 진행 중인 연쇄가 없음 (첫 피격)
	const bool bNoChain = ChainStartTime < 0.f;

	// 연쇄가 끊김. (직전 경직이 끝난 뒤 충분히 쉬었음 → 연쇄가 끊긴 것으로 봄.)
	const bool bChainBroken = LastStaggerEndTime >= 0.f && (InNow - LastStaggerEndTime) > ChainBreakTime;

	// 차단 구간까지 모두 지났음. (다음 피격은 새 연쇄)
	// 연쇄 경직 시작 시간이 차단 구간까지 넘었으면 True, 아니면 False
	const bool bChainExpired = !bNoChain && MaxChainStaggerTime > 0.f
		&& (InNow - ChainStartTime) >= MaxChainStaggerTime + ChainStaggerCooldown;

	// 새 연쇄 시작
	if (bNoChain || bChainBroken || bChainExpired)
	{
		// 연쇄 시작 시간 저장
		ChainStartTime = InNow;
	}
}
#pragma endregion

#pragma region Knockback
// 넉백 적용.
// (재발동 시에는 EndAbility 가 이전 태스크를 정리하므로 이전 넉백이 남지 않음)
void UCBHitReactAbility::ApplyKnockback()
{
	if (KnockbackDistance <= 0.f || KnockbackDuration <= 0.f) return;

	const ACharacter* Avatar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Avatar) return;

	// 플레이어는 조작감 때문에 밀지 않음 (기본값)
	if (bKnockbackAIOnly && Avatar->IsPlayerControlled()) return;

	// 공중에서는 생략. (루트모션이 수직 속도를 덮어써 낙하가 멈춤)
	const UCharacterMovementComponent* CMC = Avatar->GetCharacterMovement();
	if (!CMC || CMC->IsFalling()) return;

	// 넉백으로 밀려날 방향을 계산.
	const FVector Direction = ComputeKnockbackDirection();
	if (Direction.IsNearlyZero()) return;

	// 내비메시 밖으로 밀리면 AI 가 경로를 잃으므로 도착 예정 지점을 미리 검사.
	// (실제 이동은 CMC 가 충돌로 더 짧게 끝낼 수 있어 어디까지나 근사 검사)
	if (bClampToNavMesh)
	{
		const FVector PredictedEnd = Avatar->GetActorLocation() + Direction * KnockbackDistance;
		const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Avatar->GetWorld());

		FNavLocation NavLocation;
		if (NavSys && !NavSys->ProjectPointToNavigation(PredictedEnd, NavLocation, FVector(NavProjectExtent)))
		{
			return;
		}
	}

	// 거리 ÷ 시간 = 속도. 루트모션 소스는 CMC 브레이킹을 거치지 않으므로
	// 개이트별 감속 설정(UCBLocomotionProcessor)에 영향받지 않고 밀리는 거리가 일정함.
	const float Strength = KnockbackDistance / KnockbackDuration;

	UAbilityTask_ApplyRootMotionConstantForce* KnockbackTask =
		UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this,
			FName("CBKnockback"),
			Direction,
			Strength,
			KnockbackDuration,
			/*bIsAdditive =*/ false,
			/*StrengthOverTime =*/ nullptr,
			ERootMotionFinishVelocityMode::ClampVelocity, // 끝나는 순간 잔여 속도를 0으로 눌러 미끄러지지 않게 함
			/*SetVelocityOnFinish =*/ FVector::ZeroVector,
			/*ClampVelocityOnFinish =*/ 0.f,
			/*bEnableGravity =*/ false); // 중력은 CMC 가 그대로 처리 (지상에서만 적용되므로 영향 없음)

	if (KnockbackTask)
	{
		KnockbackTask->ReadyForActivation();
	}
}

// 넉백 방향 계산. (공격자 위치 → 타격 지점 → 자기 뒤쪽 순)
FVector UCBHitReactAbility::ComputeKnockbackDirection() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar) return FVector::ZeroVector;

	const FVector SelfLocation = Avatar->GetActorLocation();
	FVector Direction = FVector::ZeroVector;

	// 1순위: 공격자 폰 → 피격자. 거리에 흔들리지 않는 기준.
	// Instigator 는 ASC 소유 액터라 폰으로 변환.
	if (const AActor* InstigatorPawn = UCBAbilitySystemLibrary::ResolveOwningPawn(CurrentEventData.Instigator.Get()))
	{
		Direction = (SelfLocation - InstigatorPawn->GetActorLocation()).GetSafeNormal2D();
	}

	// 2순위: 무기 트레이스가 컨텍스트에 남긴 타격 지점.
	// 공격자를 못 구하는 경우(환경 데미지·폭발 등)에만 씀. 근접 공격에서는 여기까지 오지 않음.
	if (Direction.IsNearlyZero())
	{
		if (const FHitResult* HitResult = CurrentEventData.ContextHandle.GetHitResult())
		{
			Direction = (SelfLocation - HitResult->ImpactPoint).GetSafeNormal2D();
		}
	}

	// 3순위: 자기 뒤쪽 (같은 위치에서 맞는 등 방향을 못 구한 경우)
	if (Direction.IsNearlyZero())
	{
		Direction = (-Avatar->GetActorForwardVector()).GetSafeNormal2D();
	}

	return Direction;
}
#pragma endregion
