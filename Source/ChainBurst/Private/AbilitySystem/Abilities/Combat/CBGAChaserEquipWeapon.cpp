// project
#include "AbilitySystem/Abilities/Combat/CBGAChaserEquipWeapon.h"
#include "Components/Combat/CBCombatComponent.h"
#include "CBGameplayTags.h"

// engine
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"

UCBGAChaserEquipWeapon::UCBGAChaserEquipWeapon()
{
	// 로컬 입력으로 발동되어 즉시 반응해야 하므로 예측 실행
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UCBGAChaserEquipWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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
		// 컴뱃 컴포넌트의 무기 유효성 검사 (무기 없으면 장착 못함)
		if (CombatComp->HasValidWeapon() == false)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			return;
		}
		
		// 현재 전투 상태인지 확인 (이미 전투 상태라면 장착 이벤트 대기 없이 바로 종료)
		if (CombatComp->IsCombatMode() == true)
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
		
		// 무기 장착 이벤트 대기 (애님노티파이 이벤트 수신 후 장착 로직 실행)
		FGameplayTag EquipEventTag = CBGameplayTags::Event_Combat_EquipWeapon;
		EquipEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, EquipEventTag, nullptr, true);
		EquipEventTask->EventReceived.AddDynamic(this, &ThisClass::OnEquipEvent);
		EquipEventTask->ReadyForActivation();
	}
}

void UCBGAChaserEquipWeapon::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCBGAChaserEquipWeapon::OnEquipEvent(FGameplayEventData Payload)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo();

	// 전투 상태로 변경 (무기 부착)
	CombatComp->SetCombatMode(true);
	
	// 태그 추가 (로컬 적용)
	ASC->AddLooseGameplayTag(CBGameplayTags::Status_Combat_InCombat);
	
	// 서버인지 확인
	if (GetCurrentActorInfo()->IsNetAuthority())
	{
		// 태그 추가 (클라이언트에 복제)
		// UE5.7: AddReplicatedLooseGameplayTag 제거 → AddLooseGameplayTag + TagRepState 로 통합. TagAndCountToAll 이 옛 동작(태그+카운트 전원 복제)과 동일.
		ASC->AddLooseGameplayTag(CBGameplayTags::Status_Combat_InCombat, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
}
