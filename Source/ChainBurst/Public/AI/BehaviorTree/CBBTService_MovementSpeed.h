#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "GameplayTagContainer.h"
#include "CBBTService_MovementSpeed.generated.h"

/**
 * [BT] 붙은 노드가 실행되는 동안만 이동 속도 어빌리티(기본 전력질주)를 켜 두는 서비스.
 * 예: 멀리 있는 타겟에게 다가가는 MoveTo 에 붙이면 그 이동 동안 전력질주하고, MoveTo 가 끝나거나 끊기면 일반 달리기로 돌아옴.
 *
 * - 노드 진입(OnBecomeRelevant)에 켜고 이탈(OnCeaseRelevant)에 태그로 취소. 이탈은 중단(abort) 경로에서도 호출되므로 짝이 어긋나지 않음.
 * - 속도 값을 CMC 에 직접 쓰지 않고 어빌리티를 켬 - 속도·개이트 태그·가속/제동이 한 번에 따라옴 (Strafe Focus 의 걷기와 같은 경로).
 * - "멀 때"는 서비스가 판단하지 않음. 거리 조건은 붙은 가지의 데코레이터가 정함.
 *
 * 서버 전용: BT 는 서버에만 있고, 속도 GE·개이트 태그는 GAS 가 복제.
 */
UCLASS()
class CHAINBURST_API UCBBTService_MovementSpeed : public UBTService
{
	GENERATED_BODY()

public:
	UCBBTService_MovementSpeed();

protected:
	//~ Begin UBTService Interface
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	//~ End UBTService Interface

	//~ Begin UBTNode Interface
	virtual FString GetStaticDescription() const override;
	//~ End UBTNode Interface

	/**
	 * 노드가 실행되는 동안 켤 이동 속도 어빌리티의 식별 태그 (기본: 전력질주).
	 * 로드아웃에 이 태그를 가진 어빌리티(UCBGAChangeSpeed 자식)가 부여돼 있어야 함 - 없으면 속도가 바뀌지 않음.
	 */
	UPROPERTY(EditAnywhere, Category = "ChainBurst", meta = (Categories = "Ability.Movement"))
	FGameplayTag SpeedAbilityTag;
};
