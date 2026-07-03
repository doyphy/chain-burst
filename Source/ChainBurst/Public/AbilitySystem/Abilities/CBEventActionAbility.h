#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBActionAbility.h"
#include "CBEventActionAbility.generated.h"

/**
 * 게임플레이 이벤트로 트리거되는 액션(몽타주) 어빌리티 베이스
 * - 입력이 아닌 GameplayEvent(예: 피격, 사망)로 자동 발동
 * - 이벤트는 서버에서 발행되므로 기본 NetExecutionPolicy = ServerInitiated
 * - 발동 시 지정한 액션(CancelActionTag)을 캔슬하고 몽타주 재생
 * - 플레이어 / AI 구분 없이 공용으로 사용
 *
 * 자식은 생성자에서 RegisterEventTrigger()로 발동 이벤트 태그를 등록하고,
 * BoundActionTag(재생할 몽타주)와 CancelActionTag(캔슬 대상)를 설정한다.
 */
UCLASS(Abstract)
class CHAINBURST_API UCBEventActionAbility : public UCBActionAbility
{
	GENERATED_BODY()

public:
	UCBEventActionAbility();

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~ End UGameplayAbility Interface

	/**
	 * 이 어빌리티를 발동시키는 게임플레이 이벤트 태그를 트리거로 등록.
	 * 자식 생성자에서 호출한다.
	 * @param InEventTag 발동 트리거로 사용할 이벤트 태그
	 */
	void RegisterEventTrigger(const FGameplayTag& InEventTag);

	/** 발동 시 캔슬할 액션 태그 (예: Action.Combat). 비어있으면 캔슬하지 않음 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst", meta = (Categories = "Action"))
	FGameplayTag CancelActionTag;
};
