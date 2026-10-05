// project
#include "AI/BehaviorTree/CBBTDecorator_HasLineOfSight.h"
#include "Controllers/CBAIController.h"
#include "CBAbilitySystemLibrary.h"

// engine
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

UCBBTDecorator_HasLineOfSight::UCBBTDecorator_HasLineOfSight(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = TEXT("Has Line Of Sight");

	// 액터 키만 허용, 기본은 컨트롤러가 쓰는 타겟 키
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UCBBTDecorator_HasLineOfSight, BlackboardKey), AActor::StaticClass());
	BlackboardKey.SelectedKeyName = ACBAIController::TargetActorKey;

	// 시야는 블랙보드 값 변화로 알 수 없으므로 중단 옵션을 두지 않음 (엔진 IsAtLocation 과 같은 처리)
	bAllowAbortLowerPri = false;
	bAllowAbortNone = false;
	bAllowAbortChildNodes = false;
	FlowAbortMode = EBTFlowAbortMode::None;
}

// 타겟까지 벽 없이 보이는지 판정
bool UCBBTDecorator_HasLineOfSight::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const AAIController* AIOwner = OwnerComp.GetAIOwner();
	const APawn* Pawn = AIOwner ? AIOwner->GetPawn() : nullptr;
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Pawn || !Blackboard) return false;

	const AActor* TargetActor = Cast<AActor>(Blackboard->GetValue<UBlackboardKeyType_Object>(BlackboardKey.GetSelectedKeyID()));
	if (!TargetActor) return false;

	const UWorld* World = Pawn->GetWorld();
	if (!World) return false;

	// 폰 중심 → 타겟 중심. 무기 트레이스·영역 공격의 벽 검사와 같은 함수 (Weapon 채널, 캐릭터는 막지 않음)
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CBBTLineOfSight), false, Pawn);
	return !UCBAbilitySystemLibrary::IsBlockedByWall(*World, Pawn->GetActorLocation(), TargetActor->GetActorLocation(), QueryParams);
}

FString UCBBTDecorator_HasLineOfSight::GetStaticDescription() const
{
	// 베이스가 출력하는 반전 여부·키 이름을 유지하고 판정 기준 한 줄만 덧붙임
	return FString::Printf(TEXT("%s\n벽 없이 보이는가 (Weapon 채널)"), *Super::GetStaticDescription());
}
