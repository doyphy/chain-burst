// project
#include "AbilitySystem/Abilities/CBActionAbility.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem/Tasks/CBAbilityTask_WaitMontageBlendOut.h"
#include "AnimInstances/CBCharacterAnimInstance.h"
#include "Components/Animation/CBActionComponent.h"
#include "CBGameplayTags.h"

// engine
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"

void UCBActionAbility::PlayActionMontage()
{
	UCBAbilitySystemComponent* CBASC = GetCBAbilitySystemComponentFromActorInfo();
	UCBActionComponent* ActionComp = GetCBActionComponentFromActorInfo();

	// 액션 태그 유효성 검사
	if (!CBASC || !ActionComp || !BoundActionTag.IsValid())
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
	// 어빌리티 활성화는 큐 전송 컨텍스트 밖이라 이 머신에서는 여기서 바로 재생됨 → 직후에 재생 결과를 읽을 수 있음
	CBASC->ExecuteGameplayCue(CBGameplayTags::GameplayCue_PlayAction, CueParams);

	// 이 머신에서 방금 재생된 몽타주 인스턴스
	const int32 MontageInstanceID = ActionComp->GetLastMontageInstanceID();
	UCBCharacterAnimInstance* AnimInstance = ActionComp->GetAnimInstance();
	const FAnimMontageInstance* MontageInstance = AnimInstance ? AnimInstance->GetMontageInstanceForID(MontageInstanceID) : nullptr;
	if (!MontageInstance)
	{
		// 재생 실패 (몽타주 미등록 등) - 기다릴 대상이 없으므로 바로 종료
		UE_LOG(LogTemp, Warning, TEXT("[%s] 액션 몽타주 재생 실패 — 즉시 종료: %s"), *GetName(), *BoundActionTag.ToString());
		CleanupActionState();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
		return;
	}

	// 캔슬로 끝났을 때 정지시킬 대상이 있음을 표시
	bActionMontageStarted = true;

	// 마지막 프레임을 유지하는 몽타주(Auto Blend Out 꺼짐, 예: 사망)는 블렌드 아웃이 오지 않음 → 재생 직후 종료.
	// 몽타주는 큐로 재생되어 어빌리티와 무관하게 마지막 포즈를 유지함.
	if (!MontageInstance->bEnableAutoBlendOut)
	{
		CleanupActionState();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
		return;
	}

	// 몽타주 종료 이벤트 대기 (애님노티파이 수신 시 어빌리티 종료)
	EndActionTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, CBGameplayTags::Event_Action_EndAbility, nullptr, true);
	EndActionTask->EventReceived.AddDynamic(this, &ThisClass::OnActionEnded);
	EndActionTask->ReadyForActivation();

	// 몽타주 인스턴스의 블렌드 아웃 대기 (노티파이가 없거나 삼켜진 경우의 종료, 다른 몽타주에 끊긴 경우의 캔슬)
	BlendOutTask = UCBAbilityTask_WaitMontageBlendOut::WaitMontageBlendOut(this, AnimInstance, MontageInstanceID);
	BlendOutTask->OnBlendOut.AddDynamic(this, &ThisClass::OnActionMontageBlendingOut);
	BlendOutTask->ReadyForActivation();

	// 대기 등록 중에 종료됐으면 후처리하지 않음 (기다릴 인스턴스가 이미 사라진 경우)
	if (!IsActive()) return;

	// 자식 후처리 훅 (예: 입력 대기 태스크 등록)
	OnActionMontageStarted();
}

// 몽타주 종료 이벤트 대기 콜백 함수 (애님노티파이 수신 시 어빌리티 종료)
void UCBActionAbility::OnActionEnded(FGameplayEventData Payload)
{
	// 몽타주 정지 요청
	StopActionMontage();

	// 자식 상태 정리 (노티파이 지점까지 재생됨 → 체인 종료)
	CleanupActionState();

	// 어빌리티 정상 종료 - 복제하지 않음.(bReplicateEndAbility = false).
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

// 자기 몽타주 인스턴스의 블렌드 아웃 시작 콜백
void UCBActionAbility::OnActionMontageBlendingOut(bool bInterrupted)
{
	// 다른 몽타주·정지 요청에 끊김
	// 이미 현재 몽타주 정지하고 다른 몽타주로 넘어갔으므로 정지 큐를 쏘지 않음.
	if (bInterrupted)
	{
		// true 면 Montage_Stop 이 방금 시작된 새 몽타주를 끔
		bActionMontageStarted = false;

		// 캔슬로 종료해 호출자(BT 대기 태스크 등)가 실패로 식별하게 함. 자식 상태 정리는 캔슬 경로가 수행.
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
		return;
	}

	// 자식 상태 정리
	CleanupActionState();

	// 어빌리티 정상 종료 - 복제하지 않음.(bReplicateEndAbility = false).
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

// 몽타주 정지 요청 (게임플레이 큐, 전 클라 동기화).
void UCBActionAbility::StopActionMontage()
{
	// 블렌드 아웃 대기부터 끊음.
	if (BlendOutTask)
	{
		BlendOutTask->EndTask();
	}

	// 사망처럼 마지막 프레임을 유지해야 하는 액션은 정지 요청을 건너뜀 (몽타주가 그대로 남아 시체 포즈가 됨)
	if (!ShouldStopActionOnEnd()) return;

	UCBAbilitySystemComponent* CBASC = GetCBAbilitySystemComponentFromActorInfo();
	if (!CBASC) return;

	// 태스크 콜백은 예측 윈도우 밖일 수 있으므로 명시적으로 연다.
	// 없으면 소유 클라가 큐를 예측 실행하지 못해 서버 멀티캐스트를 기다리게 되고,
	// 그만큼 클라 몽타주가 더 재생되어 루트 모션이 어긋날 수 있음.
	FScopedPredictionWindow ScopedPrediction(CBASC, true);

	// 게임플레이 큐 실행 (몽타주 정지, 전 클라 동기화)
	CBASC->ExecuteGameplayCue(CBGameplayTags::GameplayCue_StopAction, FGameplayCueParameters());
}

void UCBActionAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 캔슬로 끊긴 경우 자식 상태 정리 (정상 종료 정리는 OnActionEnded / OnActionMontageBlendingOut 이 담당)
	if (bWasCancelled)
	{
		// 몽타주가 재생 중이면 정지 요청
		// 캔슬은 서버에서만 처리되므로 권한이 있는 경우에만 정지 요청
		if (bActionMontageStarted && HasAuthority(&ActivationInfo))
		{
			// 몽타주 정지 요청 (게임플레이 큐, 전 클라 동기화)
			StopActionMontage();
		}

		// 자식 상태 정리
		CleanupActionState();
	}

	// 다음 활성화를 위해 되돌림 (어빌리티는 InstancedPerActor 라 인스턴스가 재사용됨)
	bActionMontageStarted = false;
	BlendOutTask = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
