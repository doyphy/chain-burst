// project
#include "AbilitySystem/Abilities/CBChaserActionAbility.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "Components/Animation/CBActionComponent.h"
#include "CBGameplayTags.h"

// engine
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

void UCBChaserActionAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 액션 컴포넌트 가져오기
	UCBActionComponent* ActionComp = GetCBActionComponentFromActorInfo();

	// 유효성 검사
	if (!ActionComp)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	// 액션 태그가 유효하다면 (재생할 몽타주가 있다면)
	if (BoundActionTag.IsValid())
	{
		// 몽타주 재생 가능 (몽타주 재생 함수는 자식 클래스에서 호출, 재생 조건 검사 하기 위함)
		bCanPlayMontage = true;

		// 입력 상태 설정 (입력 누르면 true, 입력 떼면 false)
		bIsInputHeld = true;
	}
}

void UCBChaserActionAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	bIsInputHeld = false;
	bWaitingForInput = false;

	UE_LOG(LogTemp, Log, TEXT("Ending Ability"));

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCBChaserActionAbility::PlayActionMontage()
{
	UCBAbilitySystemComponent* CBASC = GetCBAbilitySystemComponentFromActorInfo();

	// 게임플레이 큐 데이터 만들기
	FGameplayCueParameters CueParams;

	// 재생할 액션(몽타주) 태그 담기
	CueParams.AggregatedSourceTags.AddTag(BoundActionTag);

	// 액션(몽타주)의 콤보 여부 태그 담기
	if (IsCombo)
	{
		CueParams.AggregatedSourceTags.AddTag(CBGameplayTags::Context_Action_IsCombo);
	}

	// 액션 재생 게임플레이 큐 실행 (몽타주 재생)
	CBASC->ExecuteGameplayCue(CBGameplayTags::GameplayCue_PlayAction, CueParams);

	// 내 컴퓨터의 내가 조종하고 있는 캐릭터인지 확인
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		// 입력 가능 구간 시작 이벤트 대기 (애님노티파이 이벤트 수신 후 입력 체크 시작)
		FGameplayTag CheckInputTag = CBGameplayTags::Event_Action_CheckInput;
		CheckInputTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, CheckInputTag, nullptr, true);
		CheckInputTask->EventReceived.AddDynamic(this, &ThisClass::OnCheckInput);
		CheckInputTask->ReadyForActivation();
	}
			
	// 몽타주 끝남 이벤트 대기 (애님노티파이 이벤트 수신 후 어빌리티 종료)
	FGameplayTag EndActionTag = CBGameplayTags::Event_Action_EndAbility;
	EndActionTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EndActionTag, nullptr, true);
	EndActionTask->EventReceived.AddDynamic(this, &UCBChaserActionAbility::OnActionEnded);
	EndActionTask->ReadyForActivation();
	
	// 타임 아웃 (애님노티파이 이벤트가 없는 경우, 몽타주 완전히 끝난 후 어빌리티 종료)
	DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, CurrentActionDuration());
	DelayTask->OnFinish.AddDynamic(this, &UCBChaserActionAbility::OnDelayFinished);
	DelayTask->ReadyForActivation();
}

void UCBChaserActionAbility::OnActionEnded(FGameplayEventData Payload)
{
	// 몽타주 중지 및 콤보 인덱스 초기화 여부 결정
	if (UCBActionComponent* ActionComp = GetCBActionComponentFromActorInfo())
	{
		ActionComp->StopMontage(0.25f, IsCombo);
	}
	
	// 어빌리티 종료 (bWasCancelled = false, 정상 종료 처리)
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UCBChaserActionAbility::OnCheckInput(FGameplayEventData Payload)
{
	if (bIsInputHeld)
	{
		// 아직도 입력 중이라면 즉시 재활성화
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		ASC->TryActivateAbility(CurrentSpecHandle);
	}
	else
	{
		// 입력 대기 윈도우 열기
		bWaitingForInput = true;
	}
}

void UCBChaserActionAbility::InputPressed(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	bIsInputHeld = true;

	if (bWaitingForInput)
	{
		bWaitingForInput = false;
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		ASC->TryActivateAbility(CurrentSpecHandle);
	}
}

void UCBChaserActionAbility::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	bIsInputHeld = false;
}

void UCBChaserActionAbility::OnDelayFinished()
{
	// 어빌리티 종료
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
