#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "CBBTService_UpdateTarget.generated.h"

/**
 * [BT] 주기적으로 컨트롤러에 타겟 재선정을 요청하는 서비스.
 * "언제 재평가할지"만 담당하고 "누구를 고를지"는 전부 ACBAIController::UpdateTarget() 이 판단.
 */
UCLASS()
class CHAINBURST_API UCBBTService_UpdateTarget : public UBTService
{
	GENERATED_BODY()

public:
	UCBBTService_UpdateTarget();

protected:
	//~ Begin UBTService Interface
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	//~ End UBTService Interface

	//~ Begin UBTNode Interface
	virtual FString GetStaticDescription() const override;
	//~ End UBTNode Interface
};
