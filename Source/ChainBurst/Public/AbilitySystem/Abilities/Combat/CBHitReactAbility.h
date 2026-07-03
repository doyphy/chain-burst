#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBEventActionAbility.h"
#include "CBHitReactAbility.generated.h"

/**
 * 피격 반응 어빌리티
 * - Event.Combat.HitReact 게임플레이 이벤트로 자동 발동 (입력 불필요)
 * - 발동 시 진행 중인 전투 액션(Action.Combat)을 캔슬하고 피격 몽타주(Action.Combat.HitReact) 재생
 *
 * 발동 / 캔슬 / 몽타주 재생 로직은 UCBEventActionAbility + UCBActionAbility 베이스가 처리하며,
 * 이 클래스는 태그 기본값 설정과 트리거 등록만 담당한다.
 */
UCLASS()
class CHAINBURST_API UCBHitReactAbility : public UCBEventActionAbility
{
	GENERATED_BODY()

public:
	UCBHitReactAbility();
};
