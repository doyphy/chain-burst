// project
#include "AbilitySystem/Abilities/CBChaserActionAbility.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "Components/Animation/CBActionComponent.h"

// engine
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

void UCBChaserActionAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 액션 실행
	ExecuteAction();
}

void UCBChaserActionAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UCBAbilitySystemComponent* CBASC = GetCBAbilitySystemComponentFromActorInfo())
	{
		// 입력 델리게이트 구독 해제 (입력 감지)
		CBASC->OnAbilityInputTagPressed.RemoveDynamic(this, &ThisClass::HandleInputPressed);
	}
	
	UE_LOG(LogTemp, Log, TEXT("Ending Ability"));
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCBChaserActionAbility::ExecuteAction()
{
	if (UCBAbilitySystemComponent* CBASC = GetCBAbilitySystemComponentFromActorInfo())
	{
		// 액션 태그가 유효하다면 (재생할 몽타주가 있다면)
		if (BoundActionGameplayCueTag.IsValid())
		{
			// 게임플레이 큐 실행 (몽타주 재생)
			CBASC->ExecuteGameplayCue(BoundActionGameplayCueTag);

			// 입력 가능 구간 시작 이벤트 대기 (애님노티파이 이벤트 수신 후 입력 체크 시작)
			FGameplayTag CheckInputTag = FGameplayTag::RequestGameplayTag("Event.Action.CheckInput");
			CheckInputTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
				this, CheckInputTag, nullptr, true);
			CheckInputTask->EventReceived.AddDynamic(this, &ThisClass::OnCheckInput);
			CheckInputTask->ReadyForActivation();
			
			// 몽타주 끝남 이벤트 대기 (애님노티파이 이벤트 수신 후 어빌리티 종료)
			FGameplayTag EndActionTag = FGameplayTag::RequestGameplayTag("Event.Action.EndAbility");
			EndActionTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EndActionTag, nullptr, true);
			EndActionTask->EventReceived.AddDynamic(this, &UCBChaserActionAbility::OnActionEnded);
			EndActionTask->ReadyForActivation();

			// 타임 아웃 (애님노티파이 이벤트가 없는 경우, 몽타주 완전히 끝난 후 어빌리티 종료)
			DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, CurrentActionDuration());
			DelayTask->OnFinish.AddDynamic(this, &UCBChaserActionAbility::OnDelayFinished);
			DelayTask->ReadyForActivation();
		}
	}
}

void UCBChaserActionAbility::OnActionEnded(FGameplayEventData Payload)
{
	// 콤보 초기화
	if (UCBActionComponent* ActionComp = GetCBActionComponentFromActorInfo())
	{
		ActionComp->ResetComboIndex();
	}
	
	// 어빌리티 종료 (bWasCancelled = false, 정상 종료 처리)
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UCBChaserActionAbility::OnCheckInput(FGameplayEventData Payload)
{
	if (UCBAbilitySystemComponent* CBASC = GetCBAbilitySystemComponentFromActorInfo())
	{
		// 아직도 입력중이라면 (예: 플레이어가 공격 버튼을 계속 누르고 있다면)
		if (BoundInputTag.IsValid() && CBASC->HeldInputTags.HasTagExact(BoundInputTag))
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			
			// 어빌리티 재 활성화
			CBASC->TryActivateAbility(CurrentSpecHandle);
		}
		else
		{
			// 입력 델리게이트 구독 (입력 감지)
			CBASC->OnAbilityInputTagPressed.AddDynamic(this, &ThisClass::HandleInputPressed);
		}
	}
}

void UCBChaserActionAbility::HandleInputPressed(const FGameplayTag& Data)
{
	// 같은 입력 태그가 눌렀다면
	if (Data == BoundInputTag)
	{
		// ASC 가져오기
		UCBAbilitySystemComponent* CBASC = GetCBAbilitySystemComponentFromActorInfo();
		// 어빌리티 종료
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		// 어빌리티 재 활성화
		CBASC->TryActivateAbility(CurrentSpecHandle);
	}
}

void UCBChaserActionAbility::OnDelayFinished()
{
	// 콤보 초기화
	if (UCBActionComponent* ActionComp = GetCBActionComponentFromActorInfo())
	{
		ActionComp->ResetComboIndex();
	}
	
	// 어빌리티 종료 (bWasCancelled = true, 타임아웃으로 인한 취소 처리)
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

float UCBChaserActionAbility::CurrentActionDuration() const
{
	if (UCBActionComponent* ActionComp = GetCBActionComponentFromActorInfo())
	{
		// 현재 재생중인 액션(몽타주)의 길이 반환
		return ActionComp->GetCurrentActionDuration();
	}
	return 0.0f;
}
