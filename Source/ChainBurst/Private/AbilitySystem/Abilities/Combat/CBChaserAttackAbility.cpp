// project
#include "AbilitySystem/Abilities/Combat/CBChaserAttackAbility.h"
#include "CBGameplayTags.h"
#include "Components/Combat/CBCombatComponent.h"
#include "CBAbilitySystemLibrary.h"

// engine
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"

UCBChaserAttackAbility::UCBChaserAttackAbility()
{
	// 로컬 입력으로 발동되어 즉시 반응해야 하므로 예측 실행
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UCBChaserAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                             const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                             const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 컴뱃 컴포넌트 가져오기 (베이스 UCBCombatComponent 멤버만 사용하므로 캐스팅 불필요)
	if (UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo())
	{
		// 무기 유효성 검사 (무기 없으면 공격 못함)
		if (CombatComp->HasValidWeapon() == false)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			return;
		}
		// 비전투 상태인지 확인 (비전투 상태면 공격 못함)
		if (CombatComp->IsCombatMode() == false)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			return;
		}
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 몽타주 재생 가능한지 확인
	if (bCanPlayMontage == false)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 몽타주 재생
	PlayActionMontage();
	
	// [로컬] 트레이스/히트 이벤트는 로컬에서만 발행되므로 로컬에서만 대기
	if (ActorInfo->IsLocallyControlled())
	{
		// 트레이스 시작 이벤트 대기
		UAbilityTask_WaitGameplayEvent* WaitTraceStart = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
				this,
				CBGameplayTags::Event_Combat_TraceStart,
				nullptr,
				true
		);
		WaitTraceStart->EventReceived.AddDynamic(this, &ThisClass::OnTraceStart);
		WaitTraceStart->ReadyForActivation();

		// 트레이스 종료 이벤트 대기
		UAbilityTask_WaitGameplayEvent* WaitTraceEnd = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
				this,
				CBGameplayTags::Event_Combat_TraceEnd,
				nullptr,
				true
			);
		WaitTraceEnd->EventReceived.AddDynamic(this, &ThisClass::OnTraceEnd);
		WaitTraceEnd->ReadyForActivation();

	}

	// [서버] 히트 이벤트 수신 후 GE 적용
	// Combat Comp 의 Trace 로직에서 서버에게 이벤트 전달함.
	if (ActorInfo->IsNetAuthority())
	{
		UAbilityTask_WaitGameplayEvent* WaitAttackHit = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
				this,
				CBGameplayTags::Event_Combat_Attack_Hit,
				nullptr,
				true
			);
		WaitAttackHit->EventReceived.AddDynamic(this, &ThisClass::OnAttackHit);
		WaitAttackHit->ReadyForActivation();
	}
}

void UCBChaserAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	
	// 혹시 트레이스가 아직 활성화 중이라면 강제 종료
	if (UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo())
	{
		CombatComp->StopWeaponTrace();
	}
}

void UCBChaserAttackAbility::OnTraceStart(FGameplayEventData Payload)
{
	if (UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo())
	{
		CombatComp->StartWeaponTrace();
	}
}

void UCBChaserAttackAbility::OnTraceEnd(FGameplayEventData Payload)
{
	if (UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo())
	{
		CombatComp->StopWeaponTrace();
	}
}

void UCBChaserAttackAbility::OnAttackHit(FGameplayEventData Payload)
{
	// GE 클래스 유효성 검사
	if (!DamageEffectClass) return;

	// 타겟 데이터 순회
	for (int32 i = 0; i < Payload.TargetData.Num(); i++)
	{
		// 타겟 데이터 가져오기
		const FGameplayAbilityTargetData* TargetData = Payload.TargetData.Get(i);
		if (!TargetData) continue;

		// HitResult 가져오기
		const FHitResult* HitResult = TargetData->GetHitResult();
		if (!HitResult) continue;

		// 타겟 Actor 가져오기
		AActor* HitActor = HitResult->GetActor();
		if (!HitActor) continue;

		// 타겟 Actor 의 ASC 가져오기
		UAbilitySystemComponent* TargetASC = UCBAbilitySystemLibrary::GetASC(HitActor);
		if (!TargetASC) continue;

		// Context 만들기
		FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
		ContextHandle.AddHitResult(*HitResult);

		// Spec 만들기
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());

		// 데미지 계수 설정 (SetByCaller 등록)
		SpecHandle.Data->SetSetByCallerMagnitude(CBGameplayTags::Data_Damage_Coefficient, DamageCoefficient);

		// GE 적용
		ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, Payload.TargetData);
	}
}
