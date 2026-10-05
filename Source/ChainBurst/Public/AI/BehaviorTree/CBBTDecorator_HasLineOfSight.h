#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Decorators/BTDecorator_BlackboardBase.h"
#include "CBBTDecorator_HasLineOfSight.generated.h"

/**
 * [BT] 블랙보드 타겟까지 벽 없이 보이는지 판정하는 데코레이터 (원거리 공격 등 "쏠 수 있는가" 조건).
 */
UCLASS()
class CHAINBURST_API UCBBTDecorator_HasLineOfSight : public UBTDecorator_BlackboardBase
{
	GENERATED_BODY()

public:
	UCBBTDecorator_HasLineOfSight(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UBTDecorator Interface
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	//~ End UBTDecorator Interface

	//~ Begin UBTNode Interface
	virtual FString GetStaticDescription() const override;
	//~ End UBTNode Interface
};
