// project
#include "AbilitySystem/Abilities/CBInputActionAbility.h"
#include "CBGameplayTags.h"

// engine
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameFramework/Pawn.h"

UCBInputActionAbility::UCBInputActionAbility()
{
	// 입력 트리거 어빌리티는 보통 로컬에서 발동되므로 예측 실행이 기본값
	// (구체 자식 클래스에서 필요에 따라 명시적으로 재설정)
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UCBInputActionAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 액션 컴포넌트 유효성 검사
	if (!GetCBActionComponentFromActorInfo())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 액션 태그가 유효하면 (재생할 몽타주가 있으면)
	if (BoundActionTag.IsValid())
	{
		// 몽타주 재생 가능 (실제 재생 함수는 조건 검사를 위해 자식 클래스에서 호출)
		bCanPlayMontage = true;

		// 입력 상태 설정 (입력 누르면 true, 떼면 false)
		bIsInputHeld = true;
	}
}

void UCBInputActionAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	bIsInputHeld = false;
	bWaitingForInput = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// 몽타주 재생 시작 직후 후처리 훅 (자식이 추가 작업을 수행)
void UCBInputActionAbility::OnActionMontageStarted()
{
	// 내가 조종하는 로컬 캐릭터에서만 입력 윈도우 대기 (입력 감지는 로컬에서)
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		// 입력 가능 구간 시작 이벤트 대기 (애님노티파이 수신 후 입력 체크 시작)
		CheckInputTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, CBGameplayTags::Event_Action_CheckInput, nullptr, true);
		CheckInputTask->EventReceived.AddDynamic(this, &ThisClass::OnCheckInput);
		CheckInputTask->ReadyForActivation();
	}
}

void UCBInputActionAbility::OnCheckInput(FGameplayEventData Payload)
{
	if (bIsInputHeld)
	{
		// 아직도 입력 중이면 즉시 재활성화 (콤보 연속 실행)
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		ASC->TryActivateAbility(CurrentSpecHandle);
	}
	else
	{
		// 입력 대기 윈도우 열기 (이후 InputPressed에서 재활성화 판단)
		bWaitingForInput = true;
	}
}

// 어빌리티 입력 누를 때마다 호출
void UCBInputActionAbility::InputPressed(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	bIsInputHeld = true;

	// 입력 대기 중이면 어빌리티 종료 및 재실행
	if (bWaitingForInput)
	{
		bWaitingForInput = false;
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		ASC->TryActivateAbility(CurrentSpecHandle);
	}
}

// 어빌리티 입력 뗄 때마다 호출
void UCBInputActionAbility::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	bIsInputHeld = false;
}
