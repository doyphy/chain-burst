#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "CBBTService_UpdateTargetDistance.generated.h"

/**
 * [BT] 블랙보드 타겟과의 수평 거리를 주기적으로 블랙보드 float 키에 쓰는 서비스.
 * 거리를 블랙보드 값으로 만들어, 엔진 Blackboard 데코레이터의 Observer Aborts 로 "이동 중 거리가 바뀌면 끊기"를 할 수 있게 함.
 * (엔진 IsAtLocation 은 블랙보드 값 변화에만 반응해, 액터 키는 대상이 움직여도 다시 평가되지 않음)
 * 타겟은 컨트롤러의 타겟 키(ACBAIController::TargetActorKey), 출력 키는 노드에서 선택 (기본 TargetDistance).
 * 타겟이 없으면 쓰지 않음 (전투 가지는 타겟 존재로 이미 막혀 있음).
 */
UCLASS()
class CHAINBURST_API UCBBTService_UpdateTargetDistance : public UBTService_BlackboardBase
{
	GENERATED_BODY()

public:
	UCBBTService_UpdateTargetDistance(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UBTService Interface
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	//~ End UBTService Interface

	//~ Begin UBTNode Interface
	virtual FString GetStaticDescription() const override;
	//~ End UBTNode Interface
};
