// project
#include "AbilitySystem/Abilities/Combat/CBGAChaserUnequipWeapon.h"
#include "Components/Combat/CBCombatComponent.h"
#include "CBGameplayTags.h"

// engine
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"

UCBGAChaserUnequipWeapon::UCBGAChaserUnequipWeapon()
{
	// 로컬 입력으로 발동되어 즉시 반응해야 하므로 예측 실행
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UCBGAChaserUnequipWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// ASC 가져오기
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	
	// 컴뱃 컴포넌트 가져오기
	UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo();

	if (ASC && CombatComp)
	{
		// 컴뱃 컴포넌트의 무기 유효성 검사 (무기 없으면 장착 해제 못함)
		if (CombatComp->HasValidWeapon() == false)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			return;
		}
		
		// 현재 비전투 상태인지 확인 (이미 비전투 상태라면 장착 해제 이벤트 대기 없이 바로 종료)
		if (CombatComp->IsCombatMode() == false)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			return;
		}

		// 몽타주 재생 가능인지 확인
		if (!bCanPlayMontage)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			return;
		}
		
		// 몽타주 재생
		PlayActionMontage();
		
		// 무기 장착 해제 이벤트 대기 (애님노티파이 이벤트 수신 후 장착 해제 로직 실행)
		FGameplayTag UnequipEventTag = CBGameplayTags::Event_Combat_UnequipWeapon;
		UnequipEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, UnequipEventTag, nullptr, true);
		UnequipEventTask->EventReceived.AddDynamic(this, &ThisClass::OnUnequipEvent);
		UnequipEventTask->ReadyForActivation();
	}
}

void UCBGAChaserUnequipWeapon::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCBGAChaserUnequipWeapon::OnUnequipEvent(FGameplayEventData Payload)
{
	UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo();

	// 비전투 상태로 변경 (무기 부착 + 상태 태그는 컴포넌트가 관리)
	CombatComp->SetCombatMode(false);
}
