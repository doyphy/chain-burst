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
	
	// 재생 인덱스 결정 (자식 훅. 기본 0, 콤보 액션은 자식이 콤보 인덱스를 전진시켜 반환)
	const int32 MontageIndex = SelectActionMontageIndex();

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
	// 몽타주 정지 요청 (게임플레이 큐, 전 클라 동기화)
	if (UCBAbilitySystemComponent* CBASC = GetCBAbilitySystemComponentFromActorInfo())
	{
		// 태스크 콜백은 예측 윈도우 밖일 수 있으므로 명시적으로 연다.
		// 없으면 소유 클라가 큐를 예측 실행하지 못해 서버 멀티캐스트를 기다리게 되고,
		// 그만큼 클라 몽타주가 더 재생되어 루트 모션이 어긋날 수 있음.
		FScopedPredictionWindow ScopedPrediction(CBASC, true);
		
		// 게임플레이 큐 실행 (몽타주 정지, 전 클라 동기화)
		CBASC->ExecuteGameplayCue(CBGameplayTags::GameplayCue_StopAction, FGameplayCueParameters());
	}

	// 자식 상태 정리 (몽타주가 끝까지 재생됨 → 체인 종료)
	CleanupActionState();

	// 어빌리티 정상 종료 — 복제하지 않음.(bReplicateEndAbility = false).
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

// 폴백 타임아웃 콜백 함수 (애님노티파이가 없는 경우, 몽타주 길이만큼 대기 후 종료)
void UCBActionAbility::OnDelayFinished()
{
	// 자식 상태 정리 (몽타주 길이만큼 지남 = 몽타주 끝까지 재생됨 → 체인 종료)
	CleanupActionState();

	// 어빌리티 정상 종료 — 복제하지 않음.(bReplicateEndAbility = false).
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
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
	// 캔슬로 끊긴 경우 자식 상태 정리 (정상 종료 정리는 OnActionEnded/OnDelayFinished가 담당)
	if (bWasCancelled)
	{
		CleanupActionState();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
