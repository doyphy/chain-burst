#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBEventActionAbility.h"
#include "CBHitReactAbility.generated.h"

/**
 * 피격 반응 어빌리티
 * - Event.Combat.HitReact 게임플레이 이벤트로 자동 발동 (입력 불필요)
 * - 발동 시 진행 중인 전투 액션(Action.Combat)을 캔슬하고 피격 몽타주(Action.Combat.HitReact) 재생
 * - 시전자가 Status.Combat.SuperArmor를 들고 있으면 발동 자체가 차단됨 (ActivationBlockedTags) → 스킬이 피격에 끊기지 않음
 */
UCLASS()
class CHAINBURST_API UCBHitReactAbility : public UCBEventActionAbility
{
	GENERATED_BODY()

public:
	UCBHitReactAbility();
	
protected:
	//~ Begin UCBActionAbility Interface
	virtual int32 SelectActionMontageIndex() override;
	//~ End UCBActionAbility Interface
};
