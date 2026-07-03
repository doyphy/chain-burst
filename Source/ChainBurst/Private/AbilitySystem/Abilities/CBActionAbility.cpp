// project
#include "AbilitySystem/Abilities/CBActionAbility.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "Components/Animation/CBActionComponent.h"
#include "CBGameplayTags.h"

// engine
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

void UCBActionAbility::PlayActionMontage()
{
	UCBAbilitySystemComponent* CBASC = GetCBAbilitySystemComponentFromActorInfo();

	// 액션 태그 유효성 검사
	if (!CBASC || !BoundActionTag.IsValid())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 게임플레이 큐 파라미터 구성 (재생할 액션 태그 담기)
	FGameplayCueParameters CueParams;
	CueParams.AggregatedSourceTags.AddTag(BoundActionTag);

	// 콤보 액션이면 콤보 여부 태그를 담아 콤보 몽타주가 선택되도록 함
	if (IsCombo)
	{
		CueParams.AggregatedSourceTags.AddTag(CBGameplayTags::Context_Action_IsCombo);
	}

	// 자식이 그 외 추가 태그를 붙일 수 있도록 훅 호출
	BuildActionCueParameters(CueParams);

	// 게임플레이 큐 실행 (몽타주 재생, 전 클라 동기화)
	CBASC->ExecuteGameplayCue(CBGameplayTags::GameplayCue_PlayAction, CueParams);
	
	// 몽타주 종료 이벤트 대기 (애님노티파이 수신 시 어빌리티 종료)
	EndActionTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, CBGameplayTags::Event_Action_EndAbility, nullptr, true);
	EndActionTask->EventReceived.AddDynamic(this, &ThisClass::OnActionEnded);
	EndActionTask->ReadyForActivation();

	// 폴백 타임아웃 (애님노티파이가 없는 경우, 몽타주 길이만큼 대기 후 종료)
	DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, CurrentActionDuration());
	DelayTask->OnFinish.AddDynamic(this, &ThisClass::OnDelayFinished);
	DelayTask->ReadyForActivation();

	// 자식 후처리 훅 (예: 입력 대기 태스크 등록)
	OnActionMontageStarted();
}

void UCBActionAbility::OnActionEnded(FGameplayEventData Payload)
{
	// 몽타주 정리 (블렌드 아웃 및 콤보 초기화 여부 결정)
	if (UCBActionComponent* ActionComp = GetCBActionComponentFromActorInfo())
	{
		ActionComp->StopMontage(0.25f, ShouldResetComboOnEnd());
	}

	// 어빌리티 정상 종료
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UCBActionAbility::OnDelayFinished()
{
	// 어빌리티 종료
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

float UCBActionAbility::CurrentActionDuration() const
{
	if (UCBActionComponent* ActionComp = GetCBActionComponentFromActorInfo())
	{
		// 현재 재생 중인 액션(몽타주)의 길이 반환
		return ActionComp->GetCurrentActionDuration();
	}
	return 5.f;
}
