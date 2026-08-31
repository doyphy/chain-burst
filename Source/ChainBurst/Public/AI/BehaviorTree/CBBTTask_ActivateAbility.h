#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpecHandle.h"
#include "CBBTTask_ActivateAbility.generated.h"

class UAbilitySystemComponent;

/**
 * [BT] 빙의된 폰의 ASC 에서 어빌리티를 태그로 활성화하고 즉시 결과를 반환하는 태스크.
 * 기다릴 것이 없는 어빌리티에 쓴다 (즉시 끝나는 버프·상태 전환, 또는 재생 중에도 이동을 계속해야 하는 공격)
 * 몽타주처럼 재생 시간이 있는 어빌리티는 UCBBTTask_ActivateAbilityAndWait 을 쓸 것.
 *
 * 활성화 성공 여부만 보고 끝내므로 어빌리티가 어떻게 종료됐는지(정상/캔슬)는 알지 못한다.
 *
 * 반환값:
 *  - Failed    : ASC/태그 무효, 활성화 실패 (쿨다운·차단 태그·조건 미충족·이미 활성 중)
 *  - Succeeded : 활성화 성공
 */
UCLASS()
class CHAINBURST_API UCBBTTask_ActivateAbility : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UCBBTTask_ActivateAbility();

	//~ Begin UBTTaskNode Interface
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	//~ End UBTTaskNode Interface

	//~ Begin UBTNode Interface
	virtual FString GetStaticDescription() const override;
	//~ End UBTNode Interface

protected:
	/**
	 * 빙의된 폰의 ASC 에서 AbilityTag 로 어빌리티를 활성화.
	 * 상태를 남기지 않는 const 함수이므로 공유 노드에서 호출해도 안전하다.
	 * 
	 * @param OwnerComp  실행 중인 BT 컴포넌트
	 * @param OutASC     활성화에 사용한 ASC (실패 시 건드리지 않음)
	 * @param OutHandle  활성화된 어빌리티 스펙 핸들 (실패 시 건드리지 않음)
	 * @return           활성화에 성공하면 true
	 */
	bool TryActivateAbilityByTag(const UBehaviorTreeComponent& OwnerComp, UAbilitySystemComponent*& OutASC, FGameplayAbilitySpecHandle& OutHandle) const;

	/**
	 * 빙의된 폰의 ASC 를 조회한다 (AI 는 캐릭터가 ASC 를 소유).
	 * @param OwnerComp 실행 중인 BT 컴포넌트
	 * @return 폰의 ASC. 폰이 없거나 ASC 가 없으면 nullptr
	 */
	UAbilitySystemComponent* GetPawnASC(const UBehaviorTreeComponent& OwnerComp) const;

	/**
	 * AbilityTag 어빌리티가 이미 활성 중이면 핸들을 반환 (없으면 무효 핸들).
	 * 재활성화 시도 전에 걸러내는 용도.
	 *
	 * @param InASC 검사할 ASC
	 * @return 활성 중인 어빌리티의 핸들. 없으면 무효 핸들
	 */
	FGameplayAbilitySpecHandle FindActiveAbilityHandle(const UAbilitySystemComponent& InASC) const;

	/** 활성화할 어빌리티의 태그 (어빌리티의 AssetTags 와 일치해야 함) */
	UPROPERTY(EditAnywhere, Category = "ChainBurst", meta = (Categories = "Ability"))
	FGameplayTag AbilityTag;
};
