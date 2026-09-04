// project
#include "AbilitySystem/Abilities/Combat/CBChaserAttackAbility.h"
#include "CBGameplayTags.h"
#include "Components/Combat/CBCombatComponent.h"
#include "Components/Animation/CBActionComponent.h"
#include "CBAbilitySystemLibrary.h"

// engine
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystemComponent.h"
#include "GameplayPrediction.h"

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

	// 쿨다운 GE 적용 (쿨다운 중 재활성화는 CanActivateAbility가 차단)
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
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
				false
		);
		WaitTraceStart->EventReceived.AddDynamic(this, &ThisClass::OnTraceStart);
		WaitTraceStart->ReadyForActivation();

		// 트레이스 종료 이벤트 대기
		UAbilityTask_WaitGameplayEvent* WaitTraceEnd = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
				this,
				CBGameplayTags::Event_Combat_TraceEnd,
				nullptr,
				false
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
				false
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

// 콤보 액션이면 CombatComponent의 콤보 인덱스를 전진시켜 재생할 인덱스를 가져옴.
int32 UCBChaserAttackAbility::SelectActionMontageIndex()
{
	if (!IsCombo)
	{
		return 0;
	}

	UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo();
	if (!CombatComp)
	{
		return 0;
	}

	// 최대 콤보 수 = 이 액션 태그에 등록된 몽타주 개수
	int32 MaxComboCount = 0;
	if (UCBActionComponent* ActionComp = GetCBActionComponentFromActorInfo())
	{
		MaxComboCount = ActionComp->GetMontageCount(BoundActionTag);
	}

	// 예측 키를 함께 넘겨, 거부됐을 때 정확히 이 전진만 되돌릴 수 있게 한다.
	FPredictionKey ActivationKey = CurrentActivationInfo.GetActivationPredictionKey();
	const int32 KeyValue = ActivationKey.Current;
	const int32 PlayIndex = CombatComp->AdvanceCombo(BoundActionTag, MaxComboCount, KeyValue);

	// 만약 현재 어빌리티의 예측키가 서버에서 활성화 거부되면 콤보를 되돌림.
	// 서버는 등록하지 않음.
	if (!HasAuthority(&CurrentActivationInfo))
	{
		// 현재 어빌리티가 서버에서 거부될 경우 델리게이트 등록
		TWeakObjectPtr<UCBCombatComponent> WeakCombat(CombatComp);
		ActivationKey.NewRejectedDelegate().BindWeakLambda(this, [WeakCombat, KeyValue]()
		{
			// 이전 콤보로 돌아가기
			if (UCBCombatComponent* Comp = WeakCombat.Get())
			{
				Comp->RollbackCombo(KeyValue);
			}
		});
	}

	return PlayIndex;
}

// 액션이 끝났을 때(정상·폴백·캔슬) 콤보 체인을 종료한다.
void UCBChaserAttackAbility::CleanupActionState()
{
	if (!IsCombo)
	{
		return;
	}

	if (UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo())
	{
		CombatComp->ResetCombo();
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
		// 타겟 데이터 하나씩 가져오기
		const FGameplayAbilityTargetData* TargetData = Payload.TargetData.Get(i);
		if (!TargetData) continue;

		// HitResult 가져오기
		const FHitResult* HitResult = TargetData->GetHitResult();
		if (!HitResult) continue;

		// 타겟 Actor 가져오기
		AActor* HitActor = HitResult->GetActor();
		if (!HitActor) continue;

		// 타겟 Actor 의 ASC 가져오기 (ASC 가 없는 액터는 데미지 대상이 아님)
		UAbilitySystemComponent* TargetASC = UCBAbilitySystemLibrary::GetASC(HitActor);
		if (!TargetASC) continue;

		// Spec 만들기 (소스는 시전자 ASC 로 자동 세팅됨)
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
		if (!SpecHandle.IsValid()) continue;

		// 히트 정보를 스펙 컨텍스트에 실음 (피격 방향 등 소비자용).
		SpecHandle.Data->GetContext().AddHitResult(*HitResult);

		// 데미지 계수 설정 (SetByCaller 등록)
		SpecHandle.Data->SetSetByCallerMagnitude(CBGameplayTags::Data_Damage_Coefficient, DamageCoefficient);

		// 이번 반복에서 가져온 타겟 하나만 담은 핸들.
		FGameplayAbilityTargetDataHandle SingleTargetHandle;
		SingleTargetHandle.Add(new FGameplayAbilityTargetData_SingleTargetHit(*HitResult));

		// 타겟에게 GE 적용
		ApplyGameplayEffectSpecToTarget(
			CurrentSpecHandle, // 현재 어빌리티의 SpecHandle (출처 검사용)
			CurrentActorInfo, // 현재 어빌리티의 ActorInfo (권한, 예측키 검사용 - 중복 적용 방지)
			CurrentActivationInfo, // 현재 어빌리티의 ActivationInfo (권한, 예측키 검사용 - 중복 적용 방지)
			SpecHandle,// 적용할 GE SpecHandle
			SingleTargetHandle // 타겟 데이터 (실제 GE를 적용할 타겟 ASC, HitResult 포함)
		);
	}
}
