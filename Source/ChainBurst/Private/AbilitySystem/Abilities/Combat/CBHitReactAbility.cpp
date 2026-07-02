// project
#include "AbilitySystem/Abilities/Combat/CBHitReactAbility.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "Components/Animation/CBActionComponent.h"
#include "CBGameplayTags.h"

// engine
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

UCBHitReactAbility::UCBHitReactAbility()
{
	// 기본 태그 설정
	HitReactActionTag = CBGameplayTags::Action_Combat_HitReact;
	CancelActionTag = CBGameplayTags::Action_Combat;

	// 서버에서 발행한 게임플레이 이벤트로 트리거되므로 서버 주도 실행
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// Event.Combat.HitReact 이벤트로 자동 발동되도록 트리거 등록
	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = CBGameplayTags::Event_Combat_HitReact;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

void UCBHitReactAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 진행 중인 전투 액션(공격 등) 캔슬 (휘두르던 동작 중단)
	if (UCBAbilitySystemComponent* CBASC = GetCBAbilitySystemComponentFromActorInfo())
	{
		if (CancelActionTag.IsValid())
		{
			FGameplayTagContainer CancelTags;
			CancelTags.AddTag(CancelActionTag);
			CBASC->CancelAbilities(&CancelTags, nullptr, this);
		}
	}

	// 피격 몽타주 재생
	PlayHitReactMontage();
}

void UCBHitReactAbility::PlayHitReactMontage()
{
	UCBAbilitySystemComponent* CBASC = GetCBAbilitySystemComponentFromActorInfo();
	if (!CBASC || !HitReactActionTag.IsValid())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 게임플레이 큐로 피격 몽타주 재생 (전 클라 동기화)
	FGameplayCueParameters CueParams;
	CueParams.AggregatedSourceTags.AddTag(HitReactActionTag);
	CBASC->ExecuteGameplayCue(CBGameplayTags::GameplayCue_PlayAction, CueParams);

	// 몽타주 종료 이벤트 대기 (애님노티파이 수신 시 어빌리티 종료)
	EndActionTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, CBGameplayTags::Event_Action_EndAbility, nullptr, true);
	EndActionTask->EventReceived.AddDynamic(this, &ThisClass::OnHitReactEnded);
	EndActionTask->ReadyForActivation();

	// 폴백 타임아웃 (애님노티파이가 없는 경우, 몽타주 길이만큼 대기 후 종료)
	float Duration = 5.f;
	if (UCBActionComponent* ActionComp = GetCBActionComponentFromActorInfo())
	{
		Duration = ActionComp->GetCurrentActionDuration();
	}
	DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
	DelayTask->OnFinish.AddDynamic(this, &ThisClass::OnHitReactTimeout);
	DelayTask->ReadyForActivation();
}

void UCBHitReactAbility::OnHitReactEnded(FGameplayEventData Payload)
{
	// 몽타주 정리 (블렌드 아웃)
	if (UCBActionComponent* ActionComp = GetCBActionComponentFromActorInfo())
	{
		ActionComp->StopMontage(0.25f, false);
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UCBHitReactAbility::OnHitReactTimeout()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
