// project
#include "AI/BehaviorTree/CBBTService_MovementSpeed.h"
#include "CBAbilitySystemLibrary.h"
#include "CBGameplayTags.h"

// engine
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

namespace
{
	/** BT 를 돌리는 AI 폰의 ASC */
	UAbilitySystemComponent* GetPawnASC(const UBehaviorTreeComponent& OwnerComp)
	{
		const AAIController* AIController = OwnerComp.GetAIOwner();
		return AIController ? UCBAbilitySystemLibrary::GetASC(AIController->GetPawn()) : nullptr;
	}
}

UCBBTService_MovementSpeed::UCBBTService_MovementSpeed()
{
	NodeName = TEXT("Movement Speed");

	// 노티파이 플래그는 오버라이드한 가상 함수를 보고 결정되므로 파생 클래스에서도 다시 호출해야 함.
	// TickNode 를 오버라이드하지 않으므로 틱은 돌지 않음 (진입·이탈에만 반응)
	INIT_SERVICE_NODE_NOTIFY_FLAGS();

	// 멀리 있는 타겟에게 다가갈 때 전력질주 (에디터에서 다른 개이트로 교체 가능)
	SpeedAbilityTag = CBGameplayTags::Ability_Movement_Sprint;
}

// 노드 진입: 이동 속도 어빌리티 활성화 (이미 켜져 있으면 그대로 둠)
void UCBBTService_MovementSpeed::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	UCBAbilitySystemLibrary::SetAbilityActiveByTag(GetPawnASC(OwnerComp), SpeedAbilityTag, true);
}

// 노드 이탈: 정상 완료·중단(abort) 모두 여기로 오므로 태그로 취소해 원래 개이트로 복구
void UCBBTService_MovementSpeed::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);

	UCBAbilitySystemLibrary::SetAbilityActiveByTag(GetPawnASC(OwnerComp), SpeedAbilityTag, false);
}

FString UCBBTService_MovementSpeed::GetStaticDescription() const
{
	const FString SpeedText = SpeedAbilityTag.IsValid() ? SpeedAbilityTag.ToString() : TEXT("(태그 없음)");
	return FString::Printf(TEXT("실행 중에만 속도: %s"), *SpeedText);
}
