#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "CBAbilityTask_WaitMontageBlendOut.generated.h"

class UAnimInstance;
class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCBWaitMontageBlendOutDelegate, bool, bInterrupted);

/**
 * 특정 몽타주 인스턴스(재생 1회)의 블렌드 아웃 시작을 기다리는 태스크.
 * - 몽타주 에셋이 아니라 인스턴스 ID 로 기다리므로, 같은 몽타주를 연속 재생해도 이전 재생의 이벤트를 받지 않음
 * - 이벤트를 한 번 방송하고 끝남. bInterrupted 가 true 면 다른 몽타주·정지 요청에 끊긴 것
 * - 태스크가 끝나면(어빌리티 종료 포함) 인스턴스에서 바인딩을 해제 → 이전 활성화의 이벤트가 새 활성화로 새지 않음
 */
UCLASS()
class CHAINBURST_API UCBAbilityTask_WaitMontageBlendOut : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** 블렌드 아웃 시작 시 방송 (bInterrupted : 끊겼는지 여부) */
	FCBWaitMontageBlendOutDelegate OnBlendOut;

	/**
	 * 몽타주 인스턴스의 블렌드 아웃 시작을 기다림.
	 * @param OwningAbility       태스크를 소유할 어빌리티
	 * @param InAnimInstance      몽타주가 재생 중인 애님 인스턴스
	 * @param InMontageInstanceID 기다릴 몽타주 인스턴스 ID
	 */
	static UCBAbilityTask_WaitMontageBlendOut* WaitMontageBlendOut(UGameplayAbility* OwningAbility, UAnimInstance* InAnimInstance, int32 InMontageInstanceID);

	virtual void Activate() override;

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	/** 인스턴스의 블렌드 아웃 시작 콜백 */
	void HandleBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	TWeakObjectPtr<UAnimInstance> AnimInstance;

	int32 MontageInstanceID = INDEX_NONE;
};
