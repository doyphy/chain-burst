// project
#include "AI/BehaviorTree/CBBTTask_ActivateAbilityAndWait.h"

// engine
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AbilitySystemComponent.h"

UCBBTTask_ActivateAbilityAndWait::UCBBTTask_ActivateAbilityAndWait()
{
	NodeName = TEXT("Activate Ability And Wait");

	// 어빌리티 종료 델리게이트를 AI 별로 구독해야 하므로 노드를 AI 마다 인스턴스화함.
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UCBBTTask_ActivateAbilityAndWait::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 노드 인스턴스의 멤버는 실행이 끝나도 유지되므로, 이전 실행의 잔여 구독을 먼저 정리한다.
	ClearAbilityTracking();

	UAbilitySystemComponent* ASC = GetPawnASC(OwnerComp);
	if (!ASC)
	{
		return EBTNodeResult::Failed;
	}

	// 이미 재생 중이면 다시 켜지 않고 그 어빌리티의 종료를 기다림.
	FGameplayAbilitySpecHandle TargetHandle = FindActiveAbilityHandle(*ASC);

	if (!TargetHandle.IsValid())
	{
		UAbilitySystemComponent* ActivationASC = nullptr;

		// 활성화 실패 (쿨다운·차단 태그·조건 미충족 등).
		if (!TryActivateAbilityByTag(OwnerComp, ActivationASC, TargetHandle))
		{
			return EBTNodeResult::Failed; // 실패 처리
		}

		// 활성화 직후 이미 끝난 경우. (기다릴 것이 없으므로 정상 완료)
		const FGameplayAbilitySpec* ActivatedSpec = ASC->FindAbilitySpecFromHandle(TargetHandle);
		if (!ActivatedSpec || !ActivatedSpec->IsActive())
		{
			return EBTNodeResult::Succeeded; // 성공 처리
		}
	}

	// 종료를 구독하고 대기 상태로 진입.
	CachedASC = ASC;
	CachedOwnerComp = &OwnerComp;
	ActivatedSpecHandle = TargetHandle;
	AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &UCBBTTask_ActivateAbilityAndWait::HandleAbilityEnded);

	return EBTNodeResult::InProgress; // 대기
}

// BT 가 이 태스크를 중단할 때 (상위 데코레이터 조건 변화 등) 진행 중인 어빌리티도 캔슬
EBTNodeResult::Type UCBBTTask_ActivateAbilityAndWait::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UAbilitySystemComponent* ASC = CachedASC.Get();
	const FGameplayAbilitySpecHandle HandleToCancel = ActivatedSpecHandle;

	// 어빌리티 취소 처리하기 전에 먼저 구독을 끊음.
	ClearAbilityTracking();

	if (ASC && HandleToCancel.IsValid())
	{
		// 활성화한 어빌리티 취소 처리
		ASC->CancelAbilityHandle(HandleToCancel);
	}

	return EBTNodeResult::Aborted;
}

// 노드 인스턴스가 파괴될 때 (BT 정지·AI 파괴) 남은 구독 정리
// 여기서는 어빌리티를 캔슬하지 않는 이유:
// 순서상 이미 캔슬처리됨. (StopTree는 활성 태스크의 AbortTask를 먼저 부름)
void UCBBTTask_ActivateAbilityAndWait::OnInstanceDestroyed(UBehaviorTreeComponent& OwnerComp)
{
	ClearAbilityTracking();

	Super::OnInstanceDestroyed(OwnerComp);
}

FString UCBBTTask_ActivateAbilityAndWait::GetStaticDescription() const
{
	const FString TagText = AbilityTag.IsValid() ? AbilityTag.ToString() : TEXT("(태그 없음)");

	return FString::Printf(TEXT("%s\n종료까지 대기 (캔슬 = 실패)"), *TagText);
}

// 어빌리티 종료 콜백 (이 태스크가 활성화한 어빌리티일 때만 대기를 품)
void UCBBTTask_ActivateAbilityAndWait::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	// 같은 ASC 의 다른 어빌리티(피격 반응 등) 종료는 무시
	if (EndedData.AbilitySpecHandle != ActivatedSpecHandle)
	{
		return;
	}

	UBehaviorTreeComponent* OwnerComp = CachedOwnerComp.Get();
	const bool bWasCancelled = EndedData.bWasCancelled;

	// 종료 전 구독을 끊음.
	ClearAbilityTracking();

	if (OwnerComp)
	{
		// 캔슬 종료(피격으로 끊김 등)는 공격이 성립하지 않은 것이므로 실패로 알린다
		FinishLatentTask(*OwnerComp, bWasCancelled ? EBTNodeResult::Failed : EBTNodeResult::Succeeded);
	}
}

// 종료 델리게이트 구독 해제 + 추적 상태 초기화
void UCBBTTask_ActivateAbilityAndWait::ClearAbilityTracking()
{
	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		if (AbilityEndedHandle.IsValid())
		{
			ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
		}
	}

	AbilityEndedHandle.Reset();
	CachedASC.Reset();
	CachedOwnerComp.Reset();
	ActivatedSpecHandle = FGameplayAbilitySpecHandle();
}
