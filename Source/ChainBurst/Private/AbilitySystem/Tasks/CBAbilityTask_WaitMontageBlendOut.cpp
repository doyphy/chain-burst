// project
#include "AbilitySystem/Tasks/CBAbilityTask_WaitMontageBlendOut.h"

// engine
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

UCBAbilityTask_WaitMontageBlendOut* UCBAbilityTask_WaitMontageBlendOut::WaitMontageBlendOut(UGameplayAbility* OwningAbility,
	UAnimInstance* InAnimInstance, int32 InMontageInstanceID)
{
	UCBAbilityTask_WaitMontageBlendOut* Task = NewAbilityTask<UCBAbilityTask_WaitMontageBlendOut>(OwningAbility);
	Task->AnimInstance = InAnimInstance;
	Task->MontageInstanceID = InMontageInstanceID;
	return Task;
}

void UCBAbilityTask_WaitMontageBlendOut::Activate()
{
	FAnimMontageInstance* Instance = AnimInstance.IsValid() ? AnimInstance->GetMontageInstanceForID(MontageInstanceID) : nullptr;
	if (!Instance)
	{
		// 기다릴 인스턴스가 이미 없음 → 끊긴 것으로 처리 (호스트가 영영 끝나지 않는 것을 막음)
		HandleBlendingOut(nullptr, true);
		return;
	}

	// 이 재생 1회에만 바인딩 (UObject 약참조라 태스크가 파괴된 뒤에는 호출되지 않음)
	Instance->OnMontageBlendingOutStarted.BindUObject(this, &ThisClass::HandleBlendingOut);
}

void UCBAbilityTask_WaitMontageBlendOut::OnDestroy(bool bInOwnerFinished)
{
	// 인스턴스가 아직 살아 있으면 바인딩 해제 (끝난 태스크로 이 재생의 이벤트가 오지 않게)
	if (AnimInstance.IsValid())
	{
		if (FAnimMontageInstance* Instance = AnimInstance->GetMontageInstanceForID(MontageInstanceID))
		{
			if (Instance->OnMontageBlendingOutStarted.IsBoundToObject(this))
			{
				Instance->OnMontageBlendingOutStarted.Unbind();
			}
		}
	}

	Super::OnDestroy(bInOwnerFinished);
}

void UCBAbilityTask_WaitMontageBlendOut::HandleBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnBlendOut.Broadcast(bInterrupted);
	}

	// 수신 측이 어빌리티를 끝냈다면 이미 종료됨 (EndTask 는 중복 호출에 안전)
	EndTask();
}
