// project
#include "AI/EQS/CBEnvQueryContext_Target.h"
#include "Controllers/CBAIController.h"

// engine
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "GameFramework/Pawn.h"

// 블랙보드의 TargetActor 를 기준 액터로 제공
void UCBEnvQueryContext_Target::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	// 쿼리 오너는 보통 폰이지만(bAllowControllersAsEQSQuerier 기본 false), 컨트롤러가 직접 오너인 경우도 처리함.
	UObject* QueryOwner = QueryInstance.Owner.Get();
	const APawn* QuerierPawn = Cast<APawn>(QueryOwner);
	
	const AAIController* AIController = QuerierPawn
		? Cast<AAIController>(QuerierPawn->GetController())
		: Cast<AAIController>(QueryOwner);

	// Blackboard Comp 가져오기
	const UBlackboardComponent* Blackboard = AIController ? AIController->GetBlackboardComponent() : nullptr;
	if (!Blackboard) return;

	// 키 이름은 컨트롤러의 상수를 사용 (에디터 BB 키 이름과 반드시 일치)
	AActor* TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(ACBAIController::TargetActorKey));
	if (!TargetActor) return;

	// 타겟이 없으면 여기까지 오지 않고 컨텍스트가 빈 채로 남음 → 쿼리는 아이템 없이 실패
	UEnvQueryItemType_Actor::SetContextHelper(ContextData, TargetActor);
}
