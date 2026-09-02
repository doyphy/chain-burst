// project
#include "AI/BehaviorTree/CBBTService_UpdateTarget.h"
#include "Controllers/CBAIController.h"

// engine
#include "BehaviorTree/BehaviorTreeComponent.h"

UCBBTService_UpdateTarget::UCBBTService_UpdateTarget()
{
	NodeName = TEXT("Update Target");

	// 노티파이 플래그는 오버라이드한 가상 함수를 보고 결정되므로 파생 클래스에서도 다시 호출해야 함
	INIT_SERVICE_NODE_NOTIFY_FLAGS();

	// 재평가 주기 (매 틱 X). 편차를 둬 AI 여럿의 후보 순회가 한 프레임에 몰리지 않게 함
	Interval = 0.5f;
	RandomDeviation = 0.1f;

	// 브랜치 진입 즉시 1회 평가 (주기를 기다리며 타겟 없이 서 있는 구간을 없앰)
	bCallTickOnSearchStart = true;
}

// 재선정 요청 (판단은 전부 컨트롤러가 소유, 서비스는 호출 시점만 담당)
void UCBBTService_UpdateTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	if (ACBAIController* AIController = Cast<ACBAIController>(OwnerComp.GetAIOwner()))
	{
		AIController->UpdateTarget();
	}
}

FString UCBBTService_UpdateTarget::GetStaticDescription() const
{
	// 베이스가 출력하는 주기 정보를 유지하고 역할 한 줄만 덧붙임
	return FString::Printf(TEXT("%s\n컨트롤러에 타겟 재선정 요청"), *Super::GetStaticDescription());
}
