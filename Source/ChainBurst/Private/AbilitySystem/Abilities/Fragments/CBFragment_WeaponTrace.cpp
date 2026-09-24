// project
#include "AbilitySystem/Abilities/Fragments/CBFragment_WeaponTrace.h"
#include "Components/Combat/CBCombatComponent.h"
#include "Interfaces/CBCombatInterface.h"
#include "CBAbilitySystemLibrary.h"
#include "CBGameplayTags.h"

// engine
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

bool UCBFragment_WeaponTrace::CanActivate(const FGameplayAbilityActorInfo* ActorInfo) const
{
	// 무기가 없으면 공격할 수 없음 (컴뱃 컴포넌트가 없는 캐릭터 포함)
	const UCBCombatComponent* CombatComp = GetCombatComponent(ActorInfo);
	return CombatComp && CombatComp->HasValidWeapon();
}

void UCBFragment_WeaponTrace::Start()
{
	UGameplayAbility* Ability = GetOwningAbility();
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo) return;

	// [로컬] 트레이스 구간 이벤트는 로컬에서만 발행되므로 로컬에서만 대기
	if (ActorInfo->IsLocallyControlled())
	{
		// 트레이스 시작 이벤트 대기
		UAbilityTask_WaitGameplayEvent* WaitTraceStart = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			Ability, CBGameplayTags::Event_Combat_TraceStart, nullptr, false);
		WaitTraceStart->EventReceived.AddDynamic(this, &ThisClass::OnTraceStart);
		WaitTraceStart->ReadyForActivation();

		// 트레이스 종료 이벤트 대기
		UAbilityTask_WaitGameplayEvent* WaitTraceEnd = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			Ability, CBGameplayTags::Event_Combat_TraceEnd, nullptr, false);
		WaitTraceEnd->EventReceived.AddDynamic(this, &ThisClass::OnTraceEnd);
		WaitTraceEnd->ReadyForActivation();
	}

	// [서버] 히트 이벤트 수신 후 데미지 GE 적용
	// 컴뱃 컴포넌트의 트레이스 로직이 서버에 히트 이벤트를 전달함.
	if (ActorInfo->IsNetAuthority())
	{
		UAbilityTask_WaitGameplayEvent* WaitAttackHit = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			Ability, CBGameplayTags::Event_Combat_Attack_Hit, nullptr, false);
		WaitAttackHit->EventReceived.AddDynamic(this, &ThisClass::OnAttackHit);
		WaitAttackHit->ReadyForActivation();
	}
}

void UCBFragment_WeaponTrace::Stop()
{
	// 혹시 트레이스가 아직 활성화 중이라면 강제 종료
	if (UCBCombatComponent* CombatComp = GetCombatComponent(GetOwningAbility()->GetCurrentActorInfo()))
	{
		CombatComp->StopWeaponTrace();
	}
}

UGameplayAbility* UCBFragment_WeaponTrace::GetOwningAbility() const
{
	// 호스트 어빌리티의 기본 서브오브젝트라 Outer 가 곧 소유 어빌리티
	return CastChecked<UGameplayAbility>(GetOuter());
}

UCBCombatComponent* UCBFragment_WeaponTrace::GetCombatComponent(const FGameplayAbilityActorInfo* ActorInfo)
{
	const ICBCombatInterface* CombatInterface = ActorInfo ? Cast<ICBCombatInterface>(ActorInfo->AvatarActor.Get()) : nullptr;
	return CombatInterface ? CombatInterface->GetCBCombatComponent() : nullptr;
}

void UCBFragment_WeaponTrace::OnTraceStart(FGameplayEventData Payload)
{
	if (UCBCombatComponent* CombatComp = GetCombatComponent(GetOwningAbility()->GetCurrentActorInfo()))
	{
		CombatComp->StartWeaponTrace();
	}
}

void UCBFragment_WeaponTrace::OnTraceEnd(FGameplayEventData Payload)
{
	if (UCBCombatComponent* CombatComp = GetCombatComponent(GetOwningAbility()->GetCurrentActorInfo()))
	{
		CombatComp->StopWeaponTrace();
	}
}

void UCBFragment_WeaponTrace::OnAttackHit(FGameplayEventData Payload)
{
	// GE 클래스 유효성 검사 (GE 가 없으면 피격 연출도 생략)
	if (!DamageEffectClass) return;

	UGameplayAbility* Ability = GetOwningAbility();
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!SourceASC) return;

	// 권한·예측키 검사 — 중복 적용 방지 (엔진 UGameplayAbility::ApplyGameplayEffectSpecToTarget 과 같은 게이트)
	const FGameplayAbilityActivationInfo ActivationInfo = Ability->GetCurrentActivationInfo();
	if (!Ability->HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo)) return;

	// 피격 연출 태그는 무기에 달려 있어 히트마다 바뀌지 않음. (루프 밖에서 한 번만 조회)
	FGameplayTag HitCueTag;
	if (const UCBCombatComponent* CombatComp = GetCombatComponent(ActorInfo))
	{
		HitCueTag = CombatComp->GetWeaponHitCueTag();
	}

	// 타겟 데이터 순회 (히트는 배칭되어 한 이벤트에 여러 피격자가 실려 옴)
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

		// ASC 가 없는 액터는 데미지 대상이 아님
		if (!UCBAbilitySystemLibrary::GetASC(HitActor)) continue;

		// Spec 만들기 (소스는 시전자 ASC 로 자동 세팅됨)
		FGameplayEffectSpecHandle SpecHandle = Ability->MakeOutgoingGameplayEffectSpec(DamageEffectClass, Ability->GetAbilityLevel());
		if (!SpecHandle.IsValid()) continue;

		// 히트 정보를 스펙 컨텍스트에 실음 (피격 방향 등 소비자용).
		// 스펙을 만든 뒤에 붙여야 함 — MakeOutgoingGameplayEffectSpec 이 컨텍스트를 새로 만듦.
		SpecHandle.Data->GetContext().AddHitResult(*HitResult);

		// 데미지 계수 설정 (SetByCaller 등록)
		SpecHandle.Data->SetSetByCallerMagnitude(CBGameplayTags::Data_Damage_Coefficient, DamageCoefficient);

		// 이번 반복에서 가져온 타겟 하나에게만 GE 적용.
		// 배칭된 핸들 전체를 쓰면 타겟 수만큼 도는 이 루프에서 N² 번 적용됨.
		{
			// 적용 중 어빌리티가 끝나거나 제거되지 않게 잠금 (엔진 TARGETLIST_SCOPE_LOCK 과 같음)
			FScopedTargetListLock TargetListLock(*SourceASC, *Ability);

			FGameplayAbilityTargetData_SingleTargetHit SingleTarget(*HitResult);
			SingleTarget.ApplyGameplayEffectSpec(*SpecHandle.Data.Get(), SourceASC->GetPredictionKeyForNewAction());
		}

		// 피격 연출 큐.
		UCBAbilitySystemLibrary::Auth_ExecuteHitCue(HitActor, Ability->GetAvatarActorFromActorInfo(), HitCueTag, *HitResult);
	}
}
