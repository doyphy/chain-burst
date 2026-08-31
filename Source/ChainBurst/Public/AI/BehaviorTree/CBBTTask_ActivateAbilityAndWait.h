#pragma once

#include "CoreMinimal.h"
#include "AI/BehaviorTree/CBBTTask_ActivateAbility.h"
#include "CBBTTask_ActivateAbilityAndWait.generated.h"

struct FAbilityEndedData;

/**
 * [BT] 어빌리티를 태그로 활성화하고 끝날 때까지 기다리는 태스크 (대기형).
 * 재생 중 태스크에 머무르므로 BT 가 다른 가지로 새지 않고, 종료 방식(정상 완주 / 캔슬)에 따라 반환값이 다름.
 *
 * 반환값:
 *  - Failed    : 활성화 실패(쿨다운·전제 조건 미충족 등), 캔슬 종료(피격으로 끊김 등)
 *  - Succeeded : 어빌리티가 정상 종료 (끝까지 재생 / 기다릴 것 없이 즉시 완료)
 *
 * 노드 인스턴스화(bCreateNodeInstance = true)가 필요한 이유:
 * 어빌리티 종료는 ASC 의 델리게이트로 나중에 도착하는데, 그 콜백은 노드 메모리 포인터를 받을 수 없음.
 * 콜백이 자기 AI 를 특정할 단서는 바인딩된 this 뿐이라, 노드가 AI 마다 따로 있어야 함.
 */
UCLASS()
class CHAINBURST_API UCBBTTask_ActivateAbilityAndWait : public UCBBTTask_ActivateAbility
{
	GENERATED_BODY()

public:
	UCBBTTask_ActivateAbilityAndWait();

	//~ Begin UBTTaskNode Interface
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	//~ End UBTTaskNode Interface

	//~ Begin UBTNode Interface
	virtual void OnInstanceDestroyed(UBehaviorTreeComponent& OwnerComp) override;
	virtual FString GetStaticDescription() const override;
	//~ End UBTNode Interface

private:
	/** 어빌리티 종료 콜백 (이 태스크가 활성화한 어빌리티만 처리) */
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);

	/** 종료 델리게이트 구독 해제 + 추적 상태 초기화 */
	void ClearAbilityTracking();

	/** 종료를 구독 중인 ASC */
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	/** 완료를 통지할 BT 컴포넌트 */
	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;

	/** 이 태스크가 활성화한 어빌리티 스펙 핸들 (다른 어빌리티 종료와 구분하는 기준) */
	FGameplayAbilitySpecHandle ActivatedSpecHandle;

	/** 어빌리티 종료 델리게이트 핸들 */
	FDelegateHandle AbilityEndedHandle;
};
