// project
#include "AI/BehaviorTree/CBBTService_UpdateTargetDistance.h"
#include "Controllers/CBAIController.h"

// engine
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

UCBBTService_UpdateTargetDistance::UCBBTService_UpdateTargetDistance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = TEXT("Update Target Distance");

	// 노티파이 플래그는 오버라이드한 가상 함수를 보고 결정되므로 파생 클래스에서도 다시 호출해야 함
	INIT_SERVICE_NODE_NOTIFY_FLAGS();

	// 이동 중단 반응 속도를 정하는 주기. 거리 계산 한 번이라 짧게 둠
	Interval = 0.1f;
	RandomDeviation = 0.02f;

	// 가지를 고르는 순간에도 최신 거리로 판단하도록 진입 즉시 1회 갱신
	bCallTickOnSearchStart = true;

	// float 키만 허용, 기본 키 이름
	BlackboardKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UCBBTService_UpdateTargetDistance, BlackboardKey));
	BlackboardKey.SelectedKeyName = TEXT("TargetDistance");
}

// 타겟과의 수평 거리를 블랙보드에 씀
void UCBBTService_UpdateTargetDistance::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	const AAIController* AIOwner = OwnerComp.GetAIOwner();
	const APawn* Pawn = AIOwner ? AIOwner->GetPawn() : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Pawn || !Blackboard) return;

	const AActor* TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(ACBAIController::TargetActorKey));
	if (!TargetActor) return;

	// 수평 거리 (계단·경사 등 높이 차이는 무시)
	const float Distance = FVector::Dist2D(Pawn->GetActorLocation(), TargetActor->GetActorLocation());
	Blackboard->SetValueAsFloat(BlackboardKey.SelectedKeyName, Distance);
}

FString UCBBTService_UpdateTargetDistance::GetStaticDescription() const
{
	// 베이스가 출력하는 주기 정보를 유지하고 역할 한 줄만 덧붙임
	return FString::Printf(TEXT("%s\n타겟과의 수평 거리 → %s"), *Super::GetStaticDescription(), *BlackboardKey.SelectedKeyName.ToString());
}
