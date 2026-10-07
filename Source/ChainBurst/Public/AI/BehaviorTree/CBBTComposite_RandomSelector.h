#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "CBBTComposite_RandomSelector.generated.h"

/** 랜덤 셀렉터 노드 메모리 (컴포짓 공통 메모리 + 이번 실행에서 시도한 자식) */
struct FCBRandomSelectorMemory : public FBTCompositeMemory
{
	/** 이번 실행에서 이미 시도한 자식 (비트 i = 자식 i). 노드에 들어올 때마다 비움 */
	uint32 TriedChildMask = 0;
};

/**
 * [BT] 자식을 무작위 순서로 시도하는 셀렉터.
 * 엔진 Selector 와 규칙은 같음 - 하나가 성공하면 멈추고, 전부 실패하면 실패. 시도 순서만 매번 무작위.
 * 실패한 자식(쿨다운으로 활성화 실패·데코레이터 조건 불충족 등)을 빼고 남은 자식 중에서 다시 뽑으므로,
 * 공격 후보를 넣어 두면 "지금 쓸 수 있는 공격 중 하나"를 고르게 됨. 모든 자식이 같은 확률.
 * 실행 순서가 무작위라 "낮은 우선순위" 개념이 없으므로 자식의 Lower Priority 중단은 허용하지 않음.
 * 자식은 최대 32개까지만 고려.
 */
UCLASS()
class CHAINBURST_API UCBBTComposite_RandomSelector : public UBTCompositeNode
{
	GENERATED_BODY()

public:
	UCBBTComposite_RandomSelector(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UBTCompositeNode Interface
	/** 아직 시도하지 않은 자식 중 하나를 무작위로 고름 (성공이 나오면 부모로 복귀) */
	virtual int32 GetNextChildHandler(FBehaviorTreeSearchData& SearchData, int32 PrevChild, EBTNodeResult::Type LastResult) const override;
#if WITH_EDITOR
	/** 실행 순서가 무작위라 낮은 우선순위 중단은 의미가 없음 → 에디터에서 고를 수 없게 막음 */
	virtual bool CanAbortLowerPriority() const override;
#endif
	//~ End UBTCompositeNode Interface

	//~ Begin UBTNode Interface
	virtual uint16 GetInstanceMemorySize() const override;
	virtual void InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const override;
	virtual void CleanupMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const override;
	virtual FString GetStaticDescription() const override;
#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif
	//~ End UBTNode Interface
};
