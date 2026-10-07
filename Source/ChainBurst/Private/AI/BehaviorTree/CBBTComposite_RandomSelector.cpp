// project
#include "AI/BehaviorTree/CBBTComposite_RandomSelector.h"

namespace
{
	// 시도 기록이 uint32 비트마스크라 고려할 수 있는 최대 자식 수
	constexpr int32 MaxRandomChildren = 32;
}

UCBBTComposite_RandomSelector::UCBBTComposite_RandomSelector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = TEXT("CB Random Selector");
}

// 아직 시도하지 않은 자식 중 하나를 무작위로 고름
int32 UCBBTComposite_RandomSelector::GetNextChildHandler(FBehaviorTreeSearchData& SearchData, int32 PrevChild, EBTNodeResult::Type LastResult) const
{
	FCBRandomSelectorMemory* Memory = GetNodeMemory<FCBRandomSelectorMemory>(SearchData);

	if (PrevChild == BTSpecialChild::NotInitialized)
	{
		// 새로 들어옴: 시도 기록 초기화
		Memory->TriedChildMask = 0;
	}
	else if (LastResult != EBTNodeResult::Failed)
	{
		// 성공 = 종료 (엔진 Selector 와 같은 규칙)
		return BTSpecialChild::ReturnToParent;
	}

	// 실패했거나 처음이면 아직 시도하지 않은 자식 중에서 뽑음
	// (데코레이터에 막힌 자식도 엔진이 실패로 다시 이 함수를 부르므로 같은 경로로 걸러짐)
	const int32 NumChildren = FMath::Min(GetChildrenNum(), MaxRandomChildren);
	TArray<int32, TInlineAllocator<MaxRandomChildren>> Candidates;
	for (int32 ChildIdx = 0; ChildIdx < NumChildren; ++ChildIdx)
	{
		if ((Memory->TriedChildMask & (1u << ChildIdx)) == 0)
		{
			Candidates.Add(ChildIdx);
		}
	}

	// 전부 실패 = 실패로 부모에 복귀
	if (Candidates.IsEmpty())
	{
		return BTSpecialChild::ReturnToParent;
	}

	const int32 PickedChildIdx = Candidates[FMath::RandHelper(Candidates.Num())];
	Memory->TriedChildMask |= (1u << PickedChildIdx);
	return PickedChildIdx;
}

#if WITH_EDITOR
bool UCBBTComposite_RandomSelector::CanAbortLowerPriority() const
{
	return false;
}
#endif

uint16 UCBBTComposite_RandomSelector::GetInstanceMemorySize() const
{
	return sizeof(FCBRandomSelectorMemory);
}

void UCBBTComposite_RandomSelector::InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const
{
	InitializeNodeMemory<FCBRandomSelectorMemory>(NodeMemory, InitType);
}

void UCBBTComposite_RandomSelector::CleanupMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const
{
	CleanupNodeMemory<FCBRandomSelectorMemory>(NodeMemory, CleanupType);
}

FString UCBBTComposite_RandomSelector::GetStaticDescription() const
{
	return TEXT("자식을 무작위 순서로 시도, 첫 성공에서 멈춤");
}

#if WITH_EDITOR
FName UCBBTComposite_RandomSelector::GetNodeIconName() const
{
	// 동작이 셀렉터와 같으므로 엔진 셀렉터 아이콘 사용
	return FName("BTEditor.Graph.BTNode.Composite.Selector.Icon");
}
#endif
