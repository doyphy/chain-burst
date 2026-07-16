// project
#include "AbilitySystem/Abilities/CBActionAbility.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "Components/Animation/CBActionComponent.h"
#include "Components/Combat/CBCombatComponent.h"
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
	
	// 재생 인덱스 결정
	// 비콤보 액션은 자식 훅(기본 0) - SelectActionMontageIndex()
	int32 MontageIndex = SelectActionMontageIndex();

	// 콤보 액션는 CombatComponent가 전진시킨 콤보 인덱스 - AdvanceCombo()
	if (IsCombo)
	{
		// 컴뱃 컴포넌트 가져오기
		if (UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo())
		{
			int32 MaxComboCount = 0;

			// 액션 컴포넌트 가져오기
			if (UCBActionComponent* ActionComp = GetCBActionComponentFromActorInfo())
			{
				// 최대 콤보 수 가져오기 (몽타주 개수)
				MaxComboCount = ActionComp->GetMontageCount(BoundActionTag);
			}

			// 콤보 인덱스 증가 및 이번에 재생할 콤보 인덱스 가져오기
			MontageIndex = CombatComp->AdvanceCombo(BoundActionTag, MaxComboCount);
		}
	}

	// 게임플레이 큐 파라미터 구성 (재생할 액션 태그 + 재생 인덱스)
	FGameplayCueParameters CueParams;
	CueParams.AggregatedSourceTags.AddTag(BoundActionTag);
	CueParams.RawMagnitude = static_cast<float>(MontageIndex);

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

// 몽타주 종료 이벤트 대기 콜백 함수 (애님노티파이 수신 시 어빌리티 종료)
void UCBActionAbility::OnActionEnded(FGameplayEventData Payload)
{
	// 몽타주 블렌드 아웃
	if (UCBActionComponent* ActionComp = GetCBActionComponentFromActorInfo())
	{
		ActionComp->StopMontage(0.25f);
	}

	// 콤보 리셋 (몽타주가 끝까지 재생됨 → 체인 종료)
	TryResetComboOnEnd();

	// 어빌리티 정상 종료
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// 폴백 타임아웃 콜백 함수 (애님노티파이가 없는 경우, 몽타주 길이만큼 대기 후 종료)
void UCBActionAbility::OnDelayFinished()
{
	// 콤보 리셋 (몽타주 길이만큼 지남 = 몽타주 끝까지 재생됨 → 체인 종료)
	TryResetComboOnEnd();

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

void UCBActionAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 캔슬로 끊긴 경우 콤보 리셋 (정상 종료 리셋은 OnActionEnded/OnDelayFinished가 담당,
	// 콤보 이어가기는 bWasCancelled=false로 종료되므로 여기에 안 걸림)
	if (bWasCancelled)
	{
		TryResetComboOnEnd();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCBActionAbility::TryResetComboOnEnd()
{
	// 콤보 액션일 때만 CombatComponent에 콤보 리셋 요청
	if (ShouldResetComboOnEnd())
	{
		if (UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo())
		{
			CombatComp->ResetCombo();
		}
	}
}
