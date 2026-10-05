#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "CBBTDecorator_IsPlayerBehind.generated.h"

/**
 * [BT] 등 뒤 반원·Radius 안에 살아있는 플레이어가 있는지 판정하는 데코레이터 (등 뒤 대응 공격의 조건).
 *
 * 퍼셉션이 아니라 실제 위치로 판정함 (시야는 전방뿐이고, 가만히 서서 때리는 플레이어는 소리가 없어 인지되지 않음).
 * 후보는 플레이어 폰뿐 (AI의 적이 플레이어뿐이라 진영 검사는 생략).
 */
UCLASS()
class CHAINBURST_API UCBBTDecorator_IsPlayerBehind : public UBTDecorator
{
	GENERATED_BODY()

public:
	UCBBTDecorator_IsPlayerBehind(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UBTDecorator Interface
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	//~ End UBTDecorator Interface

	//~ Begin UBTNode Interface
	virtual FString GetStaticDescription() const override;
	//~ End UBTNode Interface

	/**
	 * 등 뒤로 인정하는 수평 거리(cm). 등 뒤 대응 공격이 닿는 거리에 맞출 것.
	 * 중심 간 거리라 보스 캡슐 반경을 포함한 값으로 잡음.
	 */
	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "1.0", Units = "cm"))
	float Radius = 300.f;
};
